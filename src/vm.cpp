#include "vm.h"
#include "debug.h"

#include <stdio.h>

VM::VM() {
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

inline Value VM::read_constant_long() {
    int constant = this->read_byte();
    constant |= this->read_byte() << 8;
    constant |= this->read_byte() << 16;
    return this->chunk->constants.values[constant];
}

inline InterpretResult VM::run() {
    while (true)  {
        #ifdef DEBUG_TRACE_EXECUTION
        {
            // optionally print each instruction
            int offset = this->ip - this->chunk->code;
            this->chunk->disassemble_instruction(offset);
        }
        #endif

        uint8_t inst = this->read_byte();

        switch (inst) {
        case OP_RETURN: {
            return INTERPRET_OK;
        }
        case OP_CONSTANT: {
            Value val = this->read_constant();
            Value_print(val);
            printf("\n");
            break;
        }
        case OP_CONSTANT_LONG: {
            Value val = this->read_constant_long();
            Value_print(val);
            printf("\n");
            break;
        }
        default:
            ; // nothing
        }
    }
}
