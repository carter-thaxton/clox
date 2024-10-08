#pragma once

#include "common.h"
#include "value.h"

#define MAX_INDEX_ARCH      ((1 << 24) - 1)
#define MAX_LOCALS_ARCH     ((1 << 15) - 1)
#define MAX_UPVALUES_ARCH   ((1 << 15) - 1)

enum OpCode {
    OP_NIL,
    OP_FALSE,
    OP_TRUE,

    OP_CONSTANT,
    OP_CONSTANT_16,
    OP_CONSTANT_24,

    OP_CLASS,
    OP_CLASS_16,
    OP_CLASS_24,

    OP_METHOD,
    OP_METHOD_16,
    OP_METHOD_24,

    OP_INVOKE,
    OP_INVOKE_16,
    OP_INVOKE_24,

    OP_INVOKE_SUPER,
    OP_INVOKE_SUPER_16,
    OP_INVOKE_SUPER_24,

    OP_CLOSURE,
    OP_CLOSURE_16,
    OP_CLOSURE_24,

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

    OP_GET_UPVALUE,
    OP_GET_UPVALUE_16,
    OP_GET_UPVALUE_24,

    OP_SET_UPVALUE,
    OP_SET_UPVALUE_16,
    OP_SET_UPVALUE_24,

    OP_GET_PROPERTY,
    OP_GET_PROPERTY_16,
    OP_GET_PROPERTY_24,

    OP_SET_PROPERTY,
    OP_SET_PROPERTY_16,
    OP_SET_PROPERTY_24,

    OP_GET_SUPER,
    OP_GET_SUPER_16,
    OP_GET_SUPER_24,

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
    OP_CALL,
    OP_CLOSE_UPVALUE,
    OP_INHERIT,
};

struct Chunk {
    Chunk();
    ~Chunk();

    void write(uint8_t byte, int line);
    uint8_t read_back(int offset);

    void write_variable_length_opcode(OpCode base_op, int index, int line);
    int add_constant_value(Value value);

    uint8_t* code;
    int* lines;
    int capacity;
    int length;
    ValueArray constants;
};
