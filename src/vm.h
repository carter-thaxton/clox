#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 1024

enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
};

class VM {
public:
    VM();
    ~VM();

    InterpretResult interpret(Chunk* chunk);

    Obj* alloc_object(size_t size, ObjType type);

private:
    // some fast-path inlined functions
    InterpretResult run();
    InterpretResult runtime_error(const char* format, ...);
    void reset_stack();
    void free_objects();

    uint8_t read_byte();
    Value read_constant();
    Value read_constant_16();
    Value read_constant_24();

    void push(Value value);
    Value pop();
    Value top();
    Value peek(int depth);

    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
    Obj* objects;
};
