#pragma once

#include "common.h"
#include "value.h"
#include "chunk.h"

struct VM;

typedef Value (*NativeFn) (int argc, Value* args);

enum ObjType {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
};

struct Obj {
    ObjType type;
    Obj* next;
};

struct ObjString {
    Obj obj;
    uint32_t length;
    uint32_t hash;
    char chars[];
};

struct ObjFunction {
    Obj obj;
    ObjString* name;
    int arity;
    Chunk chunk;
};

struct ObjNative {
    Obj obj;
    NativeFn fn;
};

#define OBJ_TYPE(value)     (AS_OBJ(value)->type)

#define IS_STRING(value)    (is_obj_type(value, OBJ_STRING))
#define AS_STRING(value)    ((ObjString*) AS_OBJ(value))
#define AS_CSTRING(value)   (AS_STRING(value)->chars)

#define IS_FUNCTION(value)  (is_obj_type(value, OBJ_FUNCTION))
#define AS_FUNCTION(value)  ((ObjFunction*) AS_OBJ(value))

#define IS_NATIVE(value)    (is_obj_type(value, OBJ_NATIVE))
#define AS_NATIVE(value)    ((ObjNative*) AS_OBJ(value))

#define STRING_MAX_LEN      0x7FFFFF00


// inlined for fast-path
inline static bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

Obj* alloc_object(size_t size, ObjType type);
void free_object(Obj* object);

Value string_value(VM* vm, const char* str, int length);
Value concatenate_strings(VM* vm, Value a, Value b);

ObjFunction* new_function(VM* vm);
Value define_native(VM* vm, const char* name, NativeFn fn);
