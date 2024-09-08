#pragma once

#include "common.h"

typedef double Value;
void Value_print(Value value);  // TODO: change once Value is a class

struct ValueArray {
    ValueArray();
    ~ValueArray();

    void write(Value value);

    Value* values;
    int capacity;
    int length;
};
