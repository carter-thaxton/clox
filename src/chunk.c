
#include "chunk.h"
#include "memory.h"

void Chunk_init(Chunk* chunk) {
    chunk->code = NULL;
    chunk->capacity = 0;
    chunk->length = 0;
    ValueArray_init(&chunk->constants);
}

void Chunk_free(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    ValueArray_free(&chunk->constants);
    Chunk_init(chunk);
}

void Chunk_write(Chunk* chunk, uint8_t byte) {
    if (chunk->capacity < chunk->length + 1) {
        int old_capacity = chunk->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, new_capacity);
        chunk->capacity = new_capacity;
    }

    chunk->code[chunk->length] = byte;
    chunk->length++;
}

int Chunk_add_constant(Chunk* chunk, Value value) {
    ValueArray_write(&chunk->constants, value);
    return chunk->constants.length + 1;
}
