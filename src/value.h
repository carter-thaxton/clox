#pragma once

#include "common.h"

struct Obj;

enum ValueType {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,
};

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
};

struct ValueArray {
    ValueArray();
    ~ValueArray();

    void write(Value value);

    Value* values;
    int capacity;
    int length;
};

#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)

#define NIL_VAL             ((Value){VAL_NIL,    {.number  = 0}})
#define BOOL_VAL(value)     ((Value){VAL_BOOL,   {.boolean = value}})
#define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number  = value}})
#define OBJ_VAL(ptr)        ((Value){VAL_OBJ,    {.obj     = (Obj*) ptr}})


// inlined for fast-path
inline static bool is_truthy(Value value) {
    if (value.type == VAL_BOOL) {
        return value.as.boolean;
    } else if (value.type == VAL_NIL) {
        return false;
    } else {
        return true;
    }
}

bool values_equal(Value a, Value b);
