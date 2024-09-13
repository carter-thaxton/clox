#pragma once

#include "common.h"
#include "value.h"

#define MAX_INDEX ((1 << 24) - 1)

enum OpCode {
    OP_NIL,
    OP_FALSE,
    OP_TRUE,

    OP_CONSTANT,
    OP_CONSTANT_16,
    OP_CONSTANT_24,

    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_16,
    OP_DEFINE_GLOBAL_24,

    OP_GET_GLOBAL,
    OP_GET_GLOBAL_16,
    OP_GET_GLOBAL_24,

    OP_SET_GLOBAL,
    OP_SET_GLOBAL_16,
    OP_SET_GLOBAL_24,

    OP_GET_LOCAL,
    OP_GET_LOCAL_16,
    OP_GET_LOCAL_24,

    OP_SET_LOCAL,
    OP_SET_LOCAL_16,
    OP_SET_LOCAL_24,

    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_EQUAL,
    OP_LESS,
    OP_GREATER,

    OP_NEGATE,
    OP_NOT,

    OP_POP,
    OP_POPN,
    OP_PRINT,
    OP_RETURN,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
};

struct Chunk {
    Chunk();
    ~Chunk();

    void reset();

    void write(uint8_t byte, int line);

    void write_constant(int constant, int line);
    void write_define_global(int constant, int line);
    void write_get_global(int constant, int line);
    void write_set_global(int constant, int line);
    void write_get_local(int index, int line);
    void write_set_local(int index, int line);

    int write_constant_value(Value value, int line);
    int add_constant_value(Value value);

    uint8_t* code;
    int* lines;
    int capacity;
    int length;
    ValueArray constants;
};
