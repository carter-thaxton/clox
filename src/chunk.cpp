
#include "chunk.h"
#include "memory.h"
#include <assert.h>

Chunk::Chunk() {
    this->code = NULL;
    this->lines = NULL;
    this->capacity = 0;
    this->length = 0;
}

Chunk::~Chunk() {
    FREE_ARRAY(uint8_t, this->code, this->capacity);

    this->code = NULL;
    this->lines = NULL;
    this->capacity = 0;
    this->length = 0;
}

void Chunk::reset() {
    if (this->code)
        FREE_ARRAY(uint8_t, this->code, this->capacity);

    this->code = NULL;
    this->lines = NULL;
    this->capacity = 0;
    this->length = 0;
}

void Chunk::write(uint8_t byte, int line) {
    if (this->capacity < this->length + 1) {
        int old_capacity = this->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        this->code = GROW_ARRAY(uint8_t, this->code, old_capacity, new_capacity);
        this->lines = GROW_ARRAY(int, this->lines, old_capacity, new_capacity);
        this->capacity = new_capacity;
    }

    this->code[this->length] = byte;
    this->lines[this->length] = line;
    this->length++;
}


// Support a family of 8/16/24-bit OpCodes, which refer to constant indexes
// This assumes that base_op is the 8-bit code, with 16-bit as the next numeric opcode, followed by 24-bit
static void write_constant_op(Chunk* chunk, OpCode base_op, int constant, int line) {
    assert(constant >= 0 && constant < MAX_CONSTANTS);

    if (constant < 256) {
        // 8-bit constant index
        chunk->write(base_op, line);
        chunk->write((uint8_t) constant, line);
    } else if (constant < 65536) {
        // 16-bit constant index
        chunk->write(base_op + 1, line);
        chunk->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        chunk->write((uint8_t) (constant & 0xFF), line);
    } else {
        // 24-bit constant index
        chunk->write(base_op + 2, line);
        chunk->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        chunk->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        chunk->write((uint8_t) (constant & 0xFF), line);
    }
}


void Chunk::write_constant(int constant, int line) {
    write_constant_op(this, OP_CONSTANT, constant, line);
}

void Chunk::write_define_global(int constant, int line) {
    write_constant_op(this, OP_DEFINE_GLOBAL, constant, line);
}

int Chunk::write_constant_value(Value value, int line) {
    int constant = this->add_constant_value(value);
    write_constant(constant, line);
    return constant;
}

int Chunk::add_constant_value(Value value) {
    int index = this->constants.length;
    this->constants.write(value);
    return index;
}
