#pragma once

#include "common.h"
#include "chunk.h"

enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
};

class VM {
public:
    VM();
    ~VM();

    InterpretResult interpret(Chunk* chunk);

private:
    // some fast-path inlined functions
    InterpretResult run();
    uint8_t read_byte();
    Value read_constant();
    Value read_constant_long();

    Chunk* chunk;
    uint8_t* ip;
};
