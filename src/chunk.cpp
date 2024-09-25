
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

uint8_t Chunk::read_back(int offset) {
    assert(offset < this->length);
    return this->code[this->length - 1 - offset];
}

// Support a family of 8/16/24-bit OpCodes, which refer to a non-negative numeric index, like constants or locals.
// This assumes that base_op is the 8-bit code, with 16-bit as the next numeric opcode, followed by 24-bit
void Chunk::write_variable_length_opcode(OpCode base_op, int index, int line) {
    assert(index >= 0 && index <= MAX_INDEX);

    if (index < 256) {
        // 8-bit index
        write(base_op, line);
        write((uint8_t) index, line);
    } else if (index < 65536) {
        // 16-bit index
        write(base_op + 1, line);
        write((uint8_t) (index & 0xFF), line);
        index >>= 8;
        write((uint8_t) (index & 0xFF), line);
    } else {
        // 24-bit index
        write(base_op + 2, line);
        write((uint8_t) (index & 0xFF), line);
        index >>= 8;
        write((uint8_t) (index & 0xFF), line);
        index >>= 8;
        write((uint8_t) (index & 0xFF), line);
    }
}

int Chunk::add_constant_value(Value value) {
    // check if value has already been added
    int length = this->constants.length;
    for (int i = 0; i < length; i++) {
        if (values_equal(this->constants.values[i], value)) {
            return i;
        }
    }

    // didn't find it - add to end
    this->constants.write(value);
    return length;
}
