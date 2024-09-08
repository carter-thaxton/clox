#pragma once

#include "common.h"

typedef double Value;

typedef struct {
    Value* values;
    int capacity;
    int length;
} ValueArray;

void ValueArray_init(ValueArray* array);
void ValueArray_free(ValueArray* array);
void ValueArray_write(ValueArray* array, Value value);
