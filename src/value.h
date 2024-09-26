#pragma once

#include "common.h"
#include <string.h>

struct Obj;

enum ValueType {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,
};

#ifdef NAN_BOXING

typedef uint64_t Value;

#define SIGN_BIT            ((uint64_t) 0x8000000000000000)
#define QNAN                ((uint64_t) 0x7ffc000000000000)

#define TAG_NIL             1
#define TAG_FALSE           2
#define TAG_TRUE            3

#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NUMBER(value)    ((value & QNAN) != QNAN)
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    transmute_value_to_number(value)
#define AS_OBJ(value)       ((Obj*) ((value) & ~(QNAN | SIGN_BIT)))

#define NIL_VAL             ((Value) (QNAN | TAG_NIL))
#define FALSE_VAL           ((Value) (QNAN | TAG_FALSE))
#define TRUE_VAL            ((Value) (QNAN | TAG_TRUE))

#define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#define NUMBER_VAL(num)     transmute_number_to_value(num)
#define OBJ_VAL(ptr)        ((Value) (QNAN | SIGN_BIT | (uint64_t)(Obj*) ptr))

static inline double transmute_value_to_number(Value value) {
    double number;
    memcpy(&number, &value, sizeof(Value));
    return number;
}

static inline Value transmute_number_to_value(double number) {
    Value value;
    memcpy(&value, &number, sizeof(Value));
    return value;
}

#else

struct Value {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
};

#define IS_NIL(value)       ((value).type == VAL_NIL)
#define IS_BOOL(value)      ((value).type == VAL_BOOL)
#define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#define IS_OBJ(value)       ((value).type == VAL_OBJ)

#define AS_BOOL(value)      ((value).as.boolean)
#define AS_NUMBER(value)    ((value).as.number)
#define AS_OBJ(value)       ((value).as.obj)

#define NIL_VAL             ((Value){VAL_NIL,    {.number  = 0}})
#define BOOL_VAL(b)         ((Value){VAL_BOOL,   {.boolean = b}})
#define NUMBER_VAL(num)     ((Value){VAL_NUMBER, {.number  = num}})
#define OBJ_VAL(ptr)        ((Value){VAL_OBJ,    {.obj     = (Obj*) ptr}})

#endif

struct ValueArray {
    ValueArray();
    ~ValueArray();

    void write(Value value);
    void mark_objects();

    Value* values;
    int capacity;
    int length;
};

// inlined for fast-path
inline static bool is_truthy(Value value) {
    if (IS_BOOL(value)) {
        return AS_BOOL(value);
    } else if (IS_NIL(value)) {
        return false;
    } else {
        return true;
    }
}

bool values_equal(Value a, Value b);
void mark_value(Value value);
