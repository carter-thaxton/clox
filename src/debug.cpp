#include "debug.h"

#include <stdio.h>

static int print_simple_inst(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int print_constant_inst(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];

    printf("%-16s %4d '", name, constant);
    Value val = chunk->constants.values[constant];
    Value_print(val);
    printf("'\n");

    return offset + 2;
}

static int print_constant_long_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;

    printf("%-16s %4d '", name, constant);
    Value_print(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 4;
}

void Chunk::disassemble(const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < this->length; ) {
        offset = this->disassemble_instruction(offset);
    }
}

int Chunk::disassemble_instruction(int offset) {
    printf("%04d ", offset);

    if (offset > 0 && this->lines[offset] == this->lines[offset-1]) {
        printf("   | ");
    } else {
        printf("%4d ", this->lines[offset]);
    }

    uint8_t inst = this->code[offset];

    switch (inst) {
    case OP_CONSTANT:
        return print_constant_inst("OP_CONSTANT", this, offset);
    case OP_CONSTANT_LONG:
        return print_constant_long_inst("OP_CONSTANT_LONG", this, offset);
    case OP_RETURN:
        return print_simple_inst("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}

void Value_print(Value value) {
    printf("%g", value);
}
