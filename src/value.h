#pragma once

#include "common.h"

typedef double Value;

struct ValueArray {
    ValueArray();
    ~ValueArray();

    void write(Value value);

    Value* values;
    int capacity;
    int length;
};
