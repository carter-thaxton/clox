#include "value.h"
#include "memory.h"

void ValueArray_init(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->length = 0;
}

void ValueArray_free(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    ValueArray_init(array);
}

void ValueArray_write(ValueArray* array, Value value) {
    if (array->capacity < array->length + 1) {
        int old_capacity = array->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, new_capacity);
        array->capacity = new_capacity;
    }

    array->values[array->length] = value;
    array->length++;
}
