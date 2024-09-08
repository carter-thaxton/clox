#pragma once

#include "common.h"
#include "value.h"

enum OpCode {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_RETURN,
};

struct Chunk {
    Chunk();
    ~Chunk();

    void write(uint8_t byte, int line);
    int write_constant(Value value, int line);

    int add_constant(Value value);

    void disassemble(const char* name);
    int disassemble_instruction(int offset);

    uint8_t* code;
    int* lines;
    int capacity;
    int length;
    ValueArray constants;
};
