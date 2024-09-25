#include "vm.h"
#include "memory.h"
#include "globals.h"
#include "debug.h"
#include "compiler.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#ifdef DEBUG_STRESS_GC
#define GC_INIT_THRESHOLD   0
#define GC_GROW_FACTOR      0
#else
#define GC_INIT_THRESHOLD   1024
#define GC_GROW_FACTOR      2
#endif

VM::VM() {
    this->debug_mode = false;
    this->object_count = 0;
    this->gc_object_threshold = GC_INIT_THRESHOLD;
    this->objects = NULL;
    this->open_upvalues = NULL;
    reset_stack();
    define_globals(this);
}

VM::~VM() {
    free_all_objects();
}

InterpretResult VM::interpret(ObjFunction* main_fn) {
    assert(main_fn->obj.type == OBJ_FUNCTION);

    push(OBJ_VAL(main_fn));
    call_function(main_fn, 0);

    return run();
}

void VM::reset_stack() {
    this->stack_top = this->stack;
    this->frame_count = 0;
}

void VM::clear_globals() {
    this->globals.clear();
    define_globals(this);
}

void VM::gc() {
    #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    #endif

    mark_objects();
    mark_compiler_roots();
    strings.remove_unmarked_strings();
    int freed = sweep_objects();

    // next threshold is minimum of GC_GROW_FACTOR * object_count and GC_INIT_THRESHOLD
    int new_threshold = object_count * GC_GROW_FACTOR;
    gc_object_threshold = new_threshold < GC_INIT_THRESHOLD ? GC_INIT_THRESHOLD : new_threshold;

    #ifdef DEBUG_LOG_GC
    printf("-- gc end -- %d freed, %d remain, next at %d\n", freed, object_count, gc_object_threshold);
    #endif
}

void VM::register_object(Obj* object) {
    #ifdef DEBUG_LOG_GC
    printf("%p alloc ", (void*) object);
    print_value(OBJ_VAL(object));
    printf("\n");
    #endif

    object->next = this->objects;
    this->objects = object;
    this->object_count++;

    if (object_count >= gc_object_threshold) {
        // push/pop prevents the current object from being freed
        this->push(OBJ_VAL(object));
        gc();
        this->pop();
    }
}

void VM::free_all_objects() {
    Obj* object = this->objects;
    while (object) {
        Obj* next = object->next;
        free_object(object);
        object = next;
        object_count--;
    }
    this->objects = NULL;
    assert(object_count == 0);
}

void VM::mark_objects() {
    // stack
    for (Value* value = stack; value != stack_top; value++) {
        mark_value(*value);
    }

    // frames
    for (int i=0; i < frame_count; i++) {
        CallFrame* f = &frames[i];

        // closure recurses to function, so no need to mark both
        if (f->closure) {
            mark_object((Obj*) f->closure);
        } else {
            mark_object((Obj*) f->fn);
        }
    }

    // upvalues
    for (ObjUpvalue* upvalue = open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object((Obj*) upvalue);
    }

    // globals
    globals.mark_objects();
}

int VM::sweep_objects() {
    Obj* prev = NULL;
    Obj* object = this->objects;
    int result = 0;
    while (object) {
        if (object->marked) {
            object->marked = false;
            prev = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (prev) {
                prev->next = object;
            } else {
                this->objects = object;
            }

            #ifdef DEBUG_LOG_GC
            printf("%p free ", (void*) unreached);
            print_value(OBJ_VAL(unreached));
            printf("\n");
            #endif

            free_object(unreached);
            object_count--;
            result++;
        }
    }
    return result;
}

InterpretResult VM::runtime_error(const char* format, ...) {
    // allow variable-length args, like printf()
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // print stack-trace
    for (int i = frame_count - 1; i >= 0; i--) {
        CallFrame* frame = &frames[i];
        ObjFunction* fn = frame->fn;
        size_t inst_offset = frame->ip - fn->chunk.code - 1;
        int line = fn->chunk.lines[inst_offset];
        fprintf(stderr, "[line %d] in ", line);
        if (fn->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", fn->name->chars);
        }
    }

    reset_stack();
    return INTERPRET_RUNTIME_ERROR;
}

inline CallFrame* VM::frame() {
    assert(frame_count > 0);
    return &frames[frame_count-1];
}

inline Chunk* VM::chunk() {
    return &frame()->fn->chunk;
}

inline uint8_t VM::read_byte() {
    return *frame()->ip++;
};

inline int VM::read_unsigned_16() {
    int index = this->read_byte();
    index |= this->read_byte() << 8;
    return index;
}

inline int VM::read_unsigned_24() {
    int index = this->read_byte();
    index |= this->read_byte() << 8;
    return index;
}

inline int VM::read_signed_16() {
    int16_t index = this->read_byte();
    index |= this->read_byte() << 8;
    return (int) index;
}

inline Value VM::read_constant() {
    int constant = this->read_byte();
    return chunk()->constants.values[constant];
}

inline Value VM::read_constant_16() {
    int constant = this->read_unsigned_16();
    return chunk()->constants.values[constant];
}

inline Value VM::read_constant_24() {
    int constant = this->read_unsigned_24();
    return chunk()->constants.values[constant];
}

void VM::push(Value value) {
    *this->stack_top = value;
    this->stack_top++;
}

inline Value VM::peek(int depth) {
    return this->stack_top[-1 - depth];
}

Value VM::pop() {
    this->stack_top--;
    return *this->stack_top;
}

inline void VM::pop_n(int n) {
    this->stack_top -= n;
}

inline ObjUpvalue* VM::capture_upvalue(int index) {
    Value* value = &frame()->values[index];

    // search to see if we have already created an upvalue for this variable
    // uses the fact that upvalues references are sorted in order of the stack
    ObjUpvalue* prev = NULL;
    ObjUpvalue* upvalue = open_upvalues;
    while (upvalue != NULL && upvalue->location > value) {  // TODO: avoid using pointer comparison
        prev = upvalue;
        upvalue = upvalue->next;
    }
    if (upvalue != NULL && upvalue->location == value) {
        // already have an upvalue for this variable
        return upvalue;
    }

    if (debug_mode) {
        printf("          Creating upvalue: "); print_value(*value); printf("\n");
    }
    ObjUpvalue* created_upvalue = new_upvalue(this, value);
    created_upvalue->next = upvalue;
    if (prev != NULL) {
        prev->next = created_upvalue;
    } else {
        open_upvalues = created_upvalue;
    }

    return created_upvalue;
}

inline void VM::closure(Value fn) {
    assert(IS_FUNCTION(fn));
    ObjClosure* closure = new_closure(this, AS_FUNCTION(fn));
    push(OBJ_VAL(closure));
    for (int i=0; i < closure->upvalue_count; i++) {
        int index = read_unsigned_16();
        bool is_local = (index & 0x8000) != 0;
        index &= 0x7FFF;
        if (is_local) {
            closure->upvalues[i] = capture_upvalue(index);
        } else {
            assert(frame()->closure != NULL);
            closure->upvalues[i] = frame()->closure->upvalues[index];
        }
        assert(closure->upvalues[i] != NULL);
    }
}

inline void VM::close_upvalues(Value* last) {
    while (open_upvalues != NULL && open_upvalues->location >= last) {
        // create self-referential upvalue, so location points to value in closed
        ObjUpvalue* upvalue = open_upvalues;
        if (debug_mode) {
            printf("          Closing upvalue: "); print_value(*upvalue->location); printf("\n");
        }
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        open_upvalues = upvalue->next;
    }
}

inline InterpretResult VM::call_function(ObjFunction* fn, int argc) {
    assert(fn->upvalue_count == 0);

    if (argc != fn->arity) {
        return runtime_error("Expected %d arguments but got %d.", fn->arity, argc);
    }
    if (frame_count >= FRAME_MAX) {
        return runtime_error("Stack overflow.");
    }

    CallFrame* f = &frames[frame_count++];
    f->fn = fn;
    f->closure = NULL;
    f->ip = fn->chunk.code;
    f->values = stack_top - argc - 1;  // include args and the fn itself

    return INTERPRET_OK;
}

inline InterpretResult VM::call_closure(ObjClosure* closure, int argc) {
    if (argc != closure->fn->arity) {
        return runtime_error("Expected %d arguments but got %d.", closure->fn->arity, argc);
    }
    if (frame_count >= FRAME_MAX) {
        return runtime_error("Stack overflow.");
    }

    CallFrame* f = &frames[frame_count++];
    f->fn = closure->fn;
    f->closure = closure;
    f->ip = closure->fn->chunk.code;
    f->values = stack_top - argc - 1;  // include args and the fn itself

    return INTERPRET_OK;
}

inline InterpretResult VM::call_value(Value callee, int argc) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_FUNCTION: {
            return call_function(AS_FUNCTION(callee), argc);
        }
        case OBJ_NATIVE: {
            NativeFn native_fn = AS_NATIVE(callee)->native_fn;
            Value result = native_fn(argc, stack_top - argc);
            stack_top -= argc + 1;  // pop args and fn
            push(result);
            return INTERPRET_OK;
        }
        case OBJ_CLOSURE: {
            return call_closure(AS_CLOSURE(callee), argc);
        }
        default:
            ; // not callable
        }
    }
    return runtime_error("Can only call functions and classes.");
}

inline InterpretResult VM::run() {
    if (debug_mode) {
        printf("\n== trace ==\n");
    }

    while (true)  {
        if (debug_mode) {
            // print stack
            printf("          ");
            for (Value* slot = this->stack; slot < this->stack_top; slot++) {
                printf("[ ");
                print_value(*slot);
                printf(" ]");
            }
            printf("\n");

            // print instruction
            int offset = frame()->ip - chunk()->code;
            print_instruction(chunk(), offset);
        }

        uint8_t inst = read_byte();

        switch (inst) {

        case OP_NIL: {
            push(NIL_VAL);
            break;
        }
        case OP_FALSE: {
            push(BOOL_VAL(false));
            break;
        }
        case OP_TRUE: {
            push(BOOL_VAL(true));
            break;
        }

        case OP_CONSTANT: {
            Value val = read_constant();
            push(val);
            break;
        }
        case OP_CONSTANT_16: {
            Value val = read_constant_16();
            push(val);
            break;
        }
        case OP_CONSTANT_24: {
            Value val = read_constant_24();
            push(val);
            break;
        }

        case OP_CLOSURE: {
            Value fn = read_constant();
            closure(fn);
            break;
        }
        case OP_CLOSURE_16: {
            Value fn = read_constant_16();
            closure(fn);
            break;
        }
        case OP_CLOSURE_24: {
            Value fn = read_constant_24();
            closure(fn);
            break;
        }

        case OP_DEFINE_GLOBAL: {
            ObjString* name = AS_STRING(read_constant());
            globals.insert(name, peek(0));
            pop();
            break;
        }
        case OP_DEFINE_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant_16());
            globals.insert(name, peek(0));
            pop();
            break;
        }
        case OP_DEFINE_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant_24());
            globals.insert(name, peek(0));
            pop();
            break;
        }

        case OP_GET_GLOBAL: {
            ObjString* name = AS_STRING(read_constant());
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_GET_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant_16());
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_GET_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant_24());
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }

        case OP_SET_GLOBAL: {
            ObjString* name = AS_STRING(read_constant());
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_SET_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant_16());
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_SET_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant_24());
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }

        case OP_GET_LOCAL: {
            int index = read_byte();
            push(frame()->values[index]);
            break;
        }
        case OP_GET_LOCAL_16: {
            int index = read_unsigned_16();
            push(frame()->values[index]);
            break;
        }
        case OP_GET_LOCAL_24: {
            int index = read_unsigned_24();
            push(frame()->values[index]);
            break;
        }

        case OP_SET_LOCAL: {
            int index = read_byte();
            frame()->values[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_16: {
            int index = read_unsigned_16();
            frame()->values[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_24: {
            int index = read_unsigned_24();
            frame()->values[index] = peek(0);
            break;
        }

        case OP_GET_UPVALUE: {
            int index = read_byte();
            push(*frame()->closure->upvalues[index]->location);
            break;
        }
        case OP_GET_UPVALUE_16: {
            int index = read_unsigned_16();
            push(*frame()->closure->upvalues[index]->location);
            break;
        }
        case OP_GET_UPVALUE_24: {
            int index = read_unsigned_24();
            push(*frame()->closure->upvalues[index]->location);
            break;
        }

        case OP_SET_UPVALUE: {
            int index = read_byte();
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }
        case OP_SET_UPVALUE_16: {
            int index = read_unsigned_16();
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }
        case OP_SET_UPVALUE_24: {
            int index = read_unsigned_24();
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }

        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                Value b = peek(0);
                Value a = peek(1);
                Value result = concatenate_strings(this, a, b);
                if (IS_NIL(result)) return runtime_error("String too long.");
                pop();
                pop();
                push(result);
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else {
                return runtime_error("Operands must be two numbers or two strings.");
            }
            break;
        }
        case OP_SUBTRACT: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(a - b));
            break;
        }
        case OP_MULTIPLY: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(a * b));
            break;
        }
        case OP_DIVIDE: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(a / b));
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            bool val = values_equal(a, b);
            push(BOOL_VAL(val));
            break;
        }
        case OP_LESS: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(BOOL_VAL(a < b));
            break;
        }
        case OP_GREATER: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(BOOL_VAL(a > b));
            break;
        }

        case OP_NEGATE: {
            if (!IS_NUMBER(peek(0))) return runtime_error("Operand must be a number.");
            double val = AS_NUMBER(pop());
            push(NUMBER_VAL(-val));
            break;
        }
        case OP_NOT: {
            bool val = is_truthy(pop());
            push(BOOL_VAL(!val));
            break;
        }

        case OP_POP: {
            pop();
            break;
        }
        case OP_POPN: {
            int n = read_byte();
            pop_n(n);
            break;
        }
        case OP_PRINT: {
            Value val = pop();
            print_value(val);
            printf("\n");
            break;
        }
        case OP_RETURN: {
            Value result = pop();
            Value* frame_top = frame()->values;
            close_upvalues(frame_top);
            frame_count--;
            if (frame_count <= 0) {
                pop();  // pop main script fn
                return INTERPRET_OK;
            }
            stack_top = frame_top;
            push(result);
            break;
        }
        case OP_JUMP: {
            int jump = read_signed_16();
            frame()->ip += jump;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            int jump = read_signed_16();
            if (!is_truthy(peek(0))) {
                frame()->ip += jump;
            }
            break;
        }
        case OP_JUMP_IF_TRUE: {
            int jump = read_signed_16();
            if (is_truthy(peek(0))) {
                frame()->ip += jump;
            }
            break;
        }
        case OP_CALL: {
            int arg_count = read_byte();
            InterpretResult result = call_value(peek(arg_count), arg_count);
            if (result != INTERPRET_OK) return result;
            break;
        }
        case OP_CLOSE_UPVALUE: {
            close_upvalues(stack_top - 1);
            pop();
            break;
        }

        default:
            return runtime_error("Undefined opcode: %d", inst);
            ; // nothing
        }
    }
}
