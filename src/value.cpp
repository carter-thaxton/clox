#include "value.h"
#include "memory.h"

ValueArray::ValueArray() {
    this->values = NULL;
    this->capacity = 0;
    this->length = 0;
}

ValueArray::~ValueArray() {
    FREE_ARRAY(Value, this->values, this->capacity);

    this->values = NULL;
    this->capacity = 0;
    this->length = 0;
}

void ValueArray::write(Value value) {
    if (this->capacity < this->length + 1) {
        int old_capacity = this->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        this->values = GROW_ARRAY(Value, this->values, old_capacity, new_capacity);
        this->capacity = new_capacity;
    }

    this->values[this->length] = value;
    this->length++;
}
