#include "vm.h"
#include "memory.h"

#include "debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

VM::VM() {
    this->objects = NULL;
    this->chunk = NULL;
    this->ip = NULL;
    reset_stack();
}

VM::~VM() {
    free_objects();
    this->chunk = NULL;
    this->ip = NULL;
}

void VM::reset_stack() {
    this->stack_top = this->stack;
}

InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->ip = chunk->code;
    return run();
}

void VM::register_object(Obj* object) {
    object->next = this->objects;
    this->objects = object;
    this->object_count++;
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

    size_t inst_offset = this->ip - this->chunk->code - 1;
    int line = this->chunk->lines[inst_offset];
    fprintf(stderr, "[line %d] in script\n", line);

    reset_stack();
    return INTERPRET_RUNTIME_ERROR;
}

inline uint8_t VM::read_byte() {
    return *this->ip++;
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
    return this->chunk->constants.values[constant];
}

inline Value VM::read_constant_16() {
    int constant = this->read_unsigned_16();
    return this->chunk->constants.values[constant];
}

inline Value VM::read_constant_24() {
    int constant = this->read_unsigned_24();
    return this->chunk->constants.values[constant];
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
            int offset = this->ip - this->chunk->code;
            print_instruction(this->chunk, offset);
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
            push(this->stack[index]);
            break;
        }
        case OP_GET_LOCAL_16: {
            int index = read_unsigned_16();
            push(this->stack[index]);
            break;
        }
        case OP_GET_LOCAL_24: {
            int index = read_unsigned_24();
            push(this->stack[index]);
            break;
        }

        case OP_SET_LOCAL: {
            int index = read_byte();
            this->stack[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_16: {
            int index = read_unsigned_16();
            this->stack[index] = peek(0);
            break;
        }
        case OP_SET_LOCAL_24: {
            int index = read_unsigned_24();
            this->stack[index] = peek(0);
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
            // exit
            return INTERPRET_OK;
        }
        case OP_JUMP: {
            int jump = read_signed_16();
            this->ip += jump;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            int jump = read_signed_16();
            if (!is_truthy(peek(0))) {
                this->ip += jump;
            }
            break;
        }
        case OP_JUMP_IF_TRUE: {
            int jump = read_signed_16();
            if (is_truthy(peek(0))) {
                this->ip += jump;
            }
            break;
        }

        default:
            return runtime_error("Undefined opcode: %d", inst);
            ; // nothing
        }
    }
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
