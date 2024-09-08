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

}
