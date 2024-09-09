#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

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

    void register_object(Obj* object);

    int get_object_count() { return object_count; }
    int get_string_count() { return strings.get_count(); }
    int get_string_capacity() { return strings.get_capacity(); }

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
    Value peek(int depth);

    Chunk* chunk;
    uint8_t* ip;
    int object_count;
    Obj* objects;
    Table strings;
    Value* stack_top;
    Value stack[STACK_MAX];

    friend Value string_value(VM* vm, const char* str, int length);
    friend Value concatenate_strings(VM* vm, Value a, Value b);
};
