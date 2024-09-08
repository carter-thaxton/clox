#pragma once

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_RETURN,
} OpCode;

typedef struct {
    uint8_t* code;
    int* lines;
    int capacity;
    int length;
    ValueArray constants;
} Chunk;

void Chunk_init(Chunk* chunk);
void Chunk_free(Chunk* chunk);
void Chunk_write(Chunk* chunk, uint8_t byte, int line);
int Chunk_add_constant(Chunk* chunk, Value value);
int Chunk_write_constant(Chunk* chunk, Value value, int line);
