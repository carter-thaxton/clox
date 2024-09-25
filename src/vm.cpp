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
    this->init_string = NULL;
    clear();
}

VM::~VM() {
    this->init_string = NULL;
    reset_stack();
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

void VM::clear() {
    reset_stack();
    this->globals.clear();
    define_globals(this);
    this->init_string = AS_STRING(string_value(this, "init", 4));
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

    // strings
    mark_object((Obj*) init_string);
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

inline int VM::read_unsigned(int length) {
    if (length == 1) {
        return this->read_byte();
    } else if (length == 2) {
        return this->read_unsigned_16();
    } else if (length == 3) {
        return this->read_unsigned_24();
    } else {
        assert(!"read_unsigned length must be 1, 2, or 3");
    }
}

inline Value VM::read_constant(int length) {
    int constant;
    if (length == 1) {
        constant = this->read_byte();
    } else if (length == 2) {
        constant = this->read_unsigned_16();
    } else if (length == 3) {
        constant = this->read_unsigned_24();
    } else {
        assert(!"read_constant length must be 1, 2, or 3");
    }
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

inline void VM::define_method(ObjString* name) {
    Value method = peek(0);
    assert(IS_CLASS(peek(1)));
    ObjClass* klass = AS_CLASS(peek(1));
    klass->methods.insert(name, method);
    pop();
}

inline bool VM::bind_method(ObjClass* klass, ObjString* name) {
    Value method;
    if (!klass->methods.get(name, &method)) {
        return false;
    }

    ObjBoundMethod* bound = new_bound_method(this, peek(0), method);
    pop();
    push(OBJ_VAL(bound));
    return true;
}

inline bool VM::get_property(ObjString* name) {
    if (!IS_INSTANCE(peek(0))) {
        runtime_error("Only instances have properties.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(peek(0));
    Value val;
    if (instance->fields.get(name, &val)) {
        pop(); // instance
        push(val);
    } else if (bind_method(instance->klass, name)) {
        // success
    } else {
        runtime_error("Undefined property '%s'.", name->chars);
        return false;
    }
    return true;
}

inline bool VM::set_property(ObjString* name) {
    if (!IS_INSTANCE(peek(1))) {
        runtime_error("Only instances have fields.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(peek(1));
    instance->fields.insert(name, peek(0));
    Value val = pop();
    pop(); // instance
    push(val);
    return true;
}

inline InterpretResult VM::invoke_from_class(ObjClass* klass, ObjString* name, int argc) {
    Value method;
    if (!klass->methods.get(name, &method)) {
        return runtime_error("Undefined property '%s'.", name->chars);
    }
    return call_value(method, argc);
}

inline InterpretResult VM::invoke(ObjString* name, int argc) {
    Value receiver = peek(argc);
    if (!IS_INSTANCE(receiver)) {
        return runtime_error("Only instances have methods.");
    }
    ObjInstance* instance = AS_INSTANCE(receiver);

    Value value;
    if (instance->fields.get(name, &value)) {
        // field on the instance
        Value* location = stack_top - argc - 1;  // include args and the fn itself
        *location = value;
        return call_value(value, argc);
    }

    return invoke_from_class(instance->klass, name, argc);
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

inline InterpretResult VM::call_class(ObjClass* klass, int argc) {
    Value* location = stack_top - argc - 1;  // include args and the fn itself
    *location = OBJ_VAL(new_instance(this, klass));

    Value initializer;
    if (klass->methods.get(init_string, &initializer)) {
        return call_value(initializer, argc);
    } else if (argc != 0) {
        return runtime_error("Expected %d arguments but got %d.", 0, argc);
    }

    return INTERPRET_OK;
}

inline InterpretResult VM::call_bound_method(ObjBoundMethod* bound, int argc) {
    Value* location = stack_top - argc - 1;  // include args and the fn itself
    *location = bound->receiver;
    return call_value(bound->method, argc);
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
        case OBJ_CLASS: {
            return call_class(AS_CLASS(callee), argc);
        }
        case OBJ_BOUND_METHOD: {
            return call_bound_method(AS_BOUND_METHOD(callee), argc);
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
            Value val = read_constant(1);
            push(val);
            break;
        }
        case OP_CONSTANT_16: {
            Value val = read_constant(2);
            push(val);
            break;
        }
        case OP_CONSTANT_24: {
            Value val = read_constant(3);
            push(val);
            break;
        }

        case OP_CLASS: {
            ObjString* name = AS_STRING(read_constant(1));
            push(OBJ_VAL(new_class(this, name)));
            break;
        }
        case OP_CLASS_16: {
            ObjString* name = AS_STRING(read_constant(2));
            push(OBJ_VAL(new_class(this, name)));
            break;
        }
        case OP_CLASS_24: {
            ObjString* name = AS_STRING(read_constant(3));
            push(OBJ_VAL(new_class(this, name)));
            break;
        }

        case OP_METHOD: {
            ObjString* name = AS_STRING(read_constant(1));
            define_method(name);
            break;
        }
        case OP_METHOD_16: {
            ObjString* name = AS_STRING(read_constant(2));
            define_method(name);
            break;
        }
        case OP_METHOD_24: {
            ObjString* name = AS_STRING(read_constant(3));
            define_method(name);
            break;
        }

        case OP_INVOKE: {
            ObjString* name = AS_STRING(read_constant(1));
            int argc = read_byte();
            InterpretResult result = invoke(name, argc);
            if (result != INTERPRET_OK) return result;
            break;
        }
        case OP_INVOKE_16: {
            ObjString* name = AS_STRING(read_constant(2));
            int argc = read_byte();
            InterpretResult result = invoke(name, argc);
            if (result != INTERPRET_OK) return result;
            break;
        }
        case OP_INVOKE_24: {
            ObjString* name = AS_STRING(read_constant(3));
            int argc = read_byte();
            InterpretResult result = invoke(name, argc);
            if (result != INTERPRET_OK) return result;
            break;
        }

        case OP_CLOSURE: {
            Value fn = read_constant(1);
            closure(fn);
            break;
        }
        case OP_CLOSURE_16: {
            Value fn = read_constant(2);
            closure(fn);
            break;
        }
        case OP_CLOSURE_24: {
            Value fn = read_constant(3);
            closure(fn);
            break;
        }

        case OP_DEFINE_GLOBAL: {
            ObjString* name = AS_STRING(read_constant(1));
            globals.insert(name, peek(0));
            pop();
            break;
        }
        case OP_DEFINE_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant(2));
            globals.insert(name, peek(0));
            pop();
            break;
        }
        case OP_DEFINE_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant(3));
            globals.insert(name, peek(0));
            pop();
            break;
        }

        case OP_GET_GLOBAL: {
            ObjString* name = AS_STRING(read_constant(1));
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_GET_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant(2));
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_GET_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant(3));
            Value val;
            if (globals.get(name, &val)) {
                push(val);
            } else {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }

        case OP_SET_GLOBAL: {
            ObjString* name = AS_STRING(read_constant(1));
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_SET_GLOBAL_16: {
            ObjString* name = AS_STRING(read_constant(2));
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }
        case OP_SET_GLOBAL_24: {
            ObjString* name = AS_STRING(read_constant(3));
            if (!globals.set(name, peek(0))) {
                return runtime_error("Undefined variable '%s'.", name->chars);
            }
            break;
        }

        case OP_GET_LOCAL: {
            int index = read_unsigned(1);
            push(frame()->values[index]);
            break;
        }
        case OP_GET_LOCAL_16: {
            int index = read_unsigned(2);
            push(frame()->values[index]);
            break;
        }
        case OP_GET_LOCAL_24: {
            int index = read_unsigned(3);
            push(frame()->values[index]);
            break;
        }

        case OP_SET_LOCAL: {
            int index = read_unsigned(1);
            frame()->values[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_16: {
            int index = read_unsigned(2);
            frame()->values[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_24: {
            int index = read_unsigned(3);
            frame()->values[index] = peek(0);
            break;
        }

        case OP_GET_UPVALUE: {
            int index = read_unsigned(1);
            push(*frame()->closure->upvalues[index]->location);
            break;
        }
        case OP_GET_UPVALUE_16: {
            int index = read_unsigned(2);
            push(*frame()->closure->upvalues[index]->location);
            break;
        }
        case OP_GET_UPVALUE_24: {
            int index = read_unsigned(3);
            push(*frame()->closure->upvalues[index]->location);
            break;
        }

        case OP_SET_UPVALUE: {
            int index = read_unsigned(1);
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }
        case OP_SET_UPVALUE_16: {
            int index = read_unsigned(2);
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }
        case OP_SET_UPVALUE_24: {
            int index = read_unsigned(3);
            *frame()->closure->upvalues[index]->location = peek(0);
            break;
        }

        case OP_GET_PROPERTY: {
            ObjString* name = AS_STRING(read_constant(1));
            if (!get_property(name)) return INTERPRET_RUNTIME_ERROR;
            break;
        }
        case OP_GET_PROPERTY_16: {
            ObjString* name = AS_STRING(read_constant(2));
            if (!get_property(name)) return INTERPRET_RUNTIME_ERROR;
            break;
        }
        case OP_GET_PROPERTY_24: {
            ObjString* name = AS_STRING(read_constant(3));
            if (!get_property(name)) return INTERPRET_RUNTIME_ERROR;
            break;
        }

        case OP_SET_PROPERTY: {
            ObjString* name = AS_STRING(read_constant(1));
            if (!set_property(name)) return INTERPRET_RUNTIME_ERROR;
            break;
        }
        case OP_SET_PROPERTY_16: {
            ObjString* name = AS_STRING(read_constant(2));
            if (!set_property(name)) return INTERPRET_RUNTIME_ERROR;
            break;
        }
        case OP_SET_PROPERTY_24: {
            ObjString* name = AS_STRING(read_constant(3));
            if (!set_property(name)) return INTERPRET_RUNTIME_ERROR;
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
            int argc = read_byte();
            InterpretResult result = call_value(peek(argc), argc);
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
