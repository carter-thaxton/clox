#pragma once

#include "common.h"

enum ObjType {
    OBJ_STRING,
};

struct Obj {
    ObjType type;
};

struct ObjString {
    Obj obj;
    int length;
    char chars[];
};

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

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_STRING(value)    (is_obj_type(value, OBJ_STRING))
#define AS_STRING(value)    ((ObjString*) AS_OBJ(value))
#define AS_CSTRING(value)   (AS_STRING(value)->chars)


// some fast-path inlined functions
inline static bool is_truthy(Value value) {
    if (value.type == VAL_BOOL) {
        return value.as.boolean;
    } else if (value.type == VAL_NIL) {
        return false;
    } else {
        return true;
    }
}

inline static bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

bool values_equal(Value a, Value b);
Value make_string(const char* str, int length);
