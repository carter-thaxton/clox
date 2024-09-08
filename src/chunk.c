
#include "chunk.h"
#include "memory.h"

void Chunk_init(Chunk* chunk) {
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->capacity = 0;
    chunk->length = 0;
    ValueArray_init(&chunk->constants);
}

void Chunk_free(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    ValueArray_free(&chunk->constants);
    Chunk_init(chunk);
}

void Chunk_write(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->length + 1) {
        int old_capacity = chunk->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, new_capacity);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old_capacity, new_capacity);
        chunk->capacity = new_capacity;
    }

    chunk->code[chunk->length] = byte;
    chunk->lines[chunk->length] = line;
    chunk->length++;
}

int Chunk_write_constant(Chunk* chunk, Value value, int line) {
    int constant = Chunk_add_constant(chunk, value);
    if (constant < 256) {
        // 8-bit constant index
        Chunk_write(chunk, OP_CONSTANT, line);
        Chunk_write(chunk, (uint8_t) constant, line);
    } else {
        // 24-bit constant index
        Chunk_write(chunk, OP_CONSTANT_LONG, line);
        Chunk_write(chunk, (uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        Chunk_write(chunk, (uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        Chunk_write(chunk, (uint8_t) (constant & 0xFF), line);
    }
    return constant;
}

int Chunk_add_constant(Chunk* chunk, Value value) {
    int index = chunk->constants.length;
    ValueArray_write(&chunk->constants, value);
    return index;
}
