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
    print_value(val);
    printf("'\n");

    return offset + 2;
}

static int print_constant_long_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;

    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");

    return offset + 4;
}

void print_chunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->length; ) {
        offset = print_instruction(chunk, offset);
    }
}

int print_instruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset-1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t inst = chunk->code[offset];

    switch (inst) {
    case OP_CONSTANT:
        return print_constant_inst("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
        return print_constant_long_inst("OP_CONSTANT_LONG", chunk, offset);
    case OP_RETURN:
        return print_simple_inst("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}

void print_value(Value value) {
    printf("%g", value);
}
