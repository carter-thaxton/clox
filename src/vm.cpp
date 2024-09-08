#include "vm.h"

#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

VM::VM() {
    reset_stack();
}

VM::~VM() {
}

void VM::reset_stack() {
    this->stack_top = this->stack;
}

InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->ip = chunk->code;
    return run();
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

// some fast-path inlined functions
inline static bool is_truthy(Value val) {
    if (val.type == VAL_BOOL) {
        return val.as.boolean;
    } else if (val.type == VAL_NIL) {
        return false;
    } else {
        return true;
    }
}

inline static bool is_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        default: return false;
    }
}

inline uint8_t VM::read_byte() {
    return *this->ip++;
};

inline Value VM::read_constant() {
    int constant = this->read_byte();
    return this->chunk->constants.values[constant];
}

inline Value VM::read_constant_16() {
    int constant = this->read_byte();
    constant |= this->read_byte() << 8;
    return this->chunk->constants.values[constant];
}

inline Value VM::read_constant_24() {
    int constant = this->read_byte();
    constant |= this->read_byte() << 8;
    constant |= this->read_byte() << 16;
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
        case OP_NIL: {
            push(NIL_VAL);
            break;
        }
        case OP_TRUE: {
            push(BOOL_VAL(true));
            break;
        }
        case OP_FALSE: {
            push(BOOL_VAL(false));
            break;
        }

        case OP_ADD: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) return runtime_error("Operands must be numbers.");
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(a + b));
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
            bool val = is_equal(a, b);
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

        case OP_RETURN: {
            Value val = pop();
            print_value(val);
            printf("\n");
            return INTERPRET_OK;
        }

        default:
            ; // nothing
        }
    }
}

inline void VM::push(Value value) {
    *this->stack_top = value;
    this->stack_top++;
}

inline Value VM::pop() {
    this->stack_top--;
    return *this->stack_top;
}

inline Value VM::top() {
    return this->stack_top[-1];
}

inline Value VM::peek(int depth) {
    return this->stack_top[-1 - depth];
}

