#include "vm.h"
#include "debug.h"

#include <stdio.h>

VM::VM() {
    this->stack_top = this->stack;
}

VM::~VM() {
}

InterpretResult VM::interpret(Chunk* chunk) {
    this->chunk = chunk;
    this->ip = chunk->code;
    return run();
}

// some fast-path inlined functions
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

        uint8_t inst = this->read_byte();

        switch (inst) {
        case OP_CONSTANT: {
            Value val = this->read_constant();
            this->push(val);
            break;
        }
        case OP_CONSTANT_16: {
            Value val = this->read_constant_16();
            this->push(val);
            break;
        }
        case OP_CONSTANT_24: {
            Value val = this->read_constant_24();
            this->push(val);
            break;
        }
        case OP_ADD: {
            Value b = this->pop();
            Value a = this->pop();
            this->push(a + b);
            break;
        }
        case OP_SUBTRACT: {
            Value b = this->pop();
            Value a = this->pop();
            this->push(a - b);
            break;
        }
        case OP_MULTIPLY: {
            Value b = this->pop();
            Value a = this->pop();
            this->push(a * b);
            break;
        }
        case OP_DIVIDE: {
            Value b = this->pop();
            Value a = this->pop();
            this->push(a / b);
            break;
        }
        case OP_NEGATE: {
            Value val = this->pop();
            this->push(-val);
            break;
        }
        case OP_RETURN: {
            Value val = this->pop();
            // printf("return: ");
            // print_value(val);
            // printf("\n");
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

// inline Value VM::top() {
//     return *(this->stack_top-1);
// }

// inline void VM::replace_top(Value value) {
//     *(this->stack_top-1) = value;
// }

