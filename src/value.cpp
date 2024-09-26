#include "value.h"
#include "object.h"
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

void ValueArray::mark_objects() {
    for (int i=0; i < length; i++) {
        mark_value(values[i]);
    }
}

bool values_equal(Value a, Value b) {
    #ifdef NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    } else {
        return a == b;
    }
    #else
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
    }
    return false;
    #endif
}

void mark_value(Value value) {
    if (IS_OBJ(value)) {
        mark_object(AS_OBJ(value));
    }
}
