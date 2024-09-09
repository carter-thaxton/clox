
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

void Chunk::write_constant(int constant, int line) {
    assert(constant >= 0 && constant < MAX_CONSTANTS);

    if (constant < 256) {
        // 8-bit constant index
        this->write(OP_CONSTANT, line);
        this->write((uint8_t) constant, line);
    } else if (constant < 65536) {
        // 16-bit constant index
        this->write(OP_CONSTANT_16, line);
        this->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        this->write((uint8_t) (constant & 0xFF), line);
    } else {
        // 24-bit constant index
        this->write(OP_CONSTANT_24, line);
        this->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        this->write((uint8_t) (constant & 0xFF), line);
        constant >>= 8;
        this->write((uint8_t) (constant & 0xFF), line);
    }
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
