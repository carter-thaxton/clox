#pragma once

#include "common.h"

enum ValueType {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
};

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
    } as;

    bool is_truthy() {
        if (type == VAL_BOOL) {
            return as.boolean;
        } else if (type == VAL_NIL) {
            return false;
        } else {
            return true;
        }
    }
};

#define NIL_VAL             ((Value){VAL_NIL,    {.number  = 0}})
#define BOOL_VAL(value)     ((Value){VAL_BOOL,   {.boolean = value}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number  = value}})

#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)

struct ValueArray {
    ValueArray();
    ~ValueArray();

    void write(Value value);

    Value* values;
    int capacity;
    int length;
};
