#include "debug.h"

#include <stdio.h>

static int print_simple_inst(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static void print_constant(const char* name, Chunk* chunk, int constant) {
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
}

static int print_constant_inst(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    print_constant(name, chunk, constant);
    return offset + 2;
}

static int print_constant_16_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    print_constant(name, chunk, constant);
    return offset + 3;
}

static int print_constant_24_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;
    print_constant(name, chunk, constant);
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
    case OP_CONSTANT_16:
        return print_constant_16_inst("OP_CONSTANT_16", chunk, offset);
    case OP_CONSTANT_24:
        return print_constant_24_inst("OP_CONSTANT_24", chunk, offset);
    case OP_NIL:
        return print_simple_inst("OP_NIL", offset);
    case OP_TRUE:
        return print_simple_inst("OP_TRUE", offset);
    case OP_FALSE:
        return print_simple_inst("OP_FALSE", offset);
    case OP_ADD:
        return print_simple_inst("OP_ADD", offset);
    case OP_SUBTRACT:
        return print_simple_inst("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return print_simple_inst("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return print_simple_inst("OP_DIVIDE", offset);
    case OP_NEGATE:
        return print_simple_inst("OP_NEGATE", offset);
    case OP_RETURN:
        return print_simple_inst("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}

void print_value(Value value) {
    switch (value.type) {
        case VAL_NIL: {
            printf("nil");
            break;
        }
        case VAL_BOOL: {
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        }
        case VAL_NUMBER: {
            printf("%g", AS_NUMBER(value));
            break;
        }
        default: {
            printf("Unrecognized value type\n");
        }
    }
}
