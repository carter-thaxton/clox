#include "vm.h"
#include "memory.h"
#include "globals.h"
#include "debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

VM::VM() {
    this->objects = NULL;
    reset_stack();
    define_globals(this);
}

VM::~VM() {
    free_objects();
}

InterpretResult VM::interpret(ObjFunction* main_fn) {
    assert(main_fn->obj.type == OBJ_FUNCTION);

    push(OBJ_VAL(main_fn));
    call_function(main_fn, 0);

    return run();
}

void VM::register_object(Obj* object) {
    object->next = this->objects;
    this->objects = object;
    this->object_count++;
}

void VM::reset_stack() {
    this->stack_top = this->stack;
    this->frame_count = 0;
}

void VM::free_objects() {
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

inline void VM::push(Value value) {
    *this->stack_top = value;
    this->stack_top++;
}

inline Value VM::peek(int depth) {
    return this->stack_top[-1 - depth];
}

inline Value VM::pop() {
    this->stack_top--;
    return *this->stack_top;
}

inline void VM::pop_n(int n) {
    this->stack_top -= n;
}

inline InterpretResult VM::call_function(ObjFunction* fn, int argc) {
    if (argc != fn->arity) {
        return runtime_error("Expected %d arguments but got %d.", fn->arity, argc);
    }
    if (frame_count >= FRAME_MAX) {
        return runtime_error("Stack overflow.");
    }

    CallFrame* f = &frames[frame_count++];
    f->fn = fn;
    f->ip = fn->chunk.code;
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
            NativeFn fn = AS_NATIVE(callee)->fn;
            Value result = fn(argc, stack_top - argc);
            stack_top -= argc + 1;  // pop args and fn
            push(result);
            return INTERPRET_OK;
        }
        default:
            ; // not callable
        }
    }
    return runtime_error("Can only call functions and classes.");
}

inline InterpretResult VM::run() {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("\n== trace ==\n");
    #endif

    while (true)  {
        #ifdef DEBUG_TRACE_EXECUTION
        {
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
        #endif

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

        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                Value b = pop();
                Value a = pop();
                Value result = concatenate_strings(this, a, b);
                if (IS_NIL(result)) return runtime_error("String too long.");
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

        default:
            return runtime_error("Undefined opcode: %d", inst);
            ; // nothing
        }
    }
}
