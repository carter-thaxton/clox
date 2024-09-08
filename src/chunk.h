#pragma once

#include "common.h"
#include "value.h"

#define MAX_CONSTANTS (1 << 24)

enum OpCode {
    OP_CONSTANT,
    OP_CONSTANT_16,
    OP_CONSTANT_24,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN,
};

struct Chunk {
    Chunk();
    ~Chunk();

    void write(uint8_t byte, int line);
    int write_constant(Value value, int line);

    int add_constant(Value value);

    uint8_t* code;
    int* lines;
    int capacity;
    int length;
    ValueArray constants;
};
