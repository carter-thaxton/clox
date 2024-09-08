#pragma once

#include "common.h"

typedef enum {
    OP_RETURN,
} OpCode;

typedef struct {
    uint8_t* code;
    int capacity;
    int length;
} Chunk;

void Chunk_init(Chunk *chunk);
void Chunk_free(Chunk *chunk);
void Chunk_write(Chunk* chunk, uint8_t byte);
