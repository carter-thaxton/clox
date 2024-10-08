#pragma once

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

struct VM;

typedef Value (*NativeFn) (int argc, Value* args);

enum ObjType {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_UPVALUE,
    OBJ_CLOSURE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
};

struct Obj {
    ObjType type;
    bool marked;
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
    uint32_t arity;
    uint32_t upvalue_count;
    Chunk chunk;
};

struct ObjNative {
    Obj obj;
    NativeFn native_fn;
};

struct ObjUpvalue {
    Obj obj;
    Value closed;
    Value* location;
    ObjUpvalue* next;
};

struct ObjClosure {
    Obj obj;
    ObjFunction* fn;
    uint32_t upvalue_count;
    ObjUpvalue* upvalues[];
};

struct ObjClass {
    Obj obj;
    ObjString* name;
    Table methods;
};

struct ObjInstance {
    Obj obj;
    ObjClass* klass;
    Table fields;
};

struct ObjBoundMethod {
    Obj obj;
    Value receiver;
    Value method;  // function or closure
};

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)

#define IS_STRING(value)        (is_obj_type(value, OBJ_STRING))
#define AS_STRING(value)        ((ObjString*) AS_OBJ(value))
#define AS_CSTRING(value)       (AS_STRING(value)->chars)

#define IS_FUNCTION(value)      (is_obj_type(value, OBJ_FUNCTION))
#define AS_FUNCTION(value)      ((ObjFunction*) AS_OBJ(value))

#define IS_NATIVE(value)        (is_obj_type(value, OBJ_NATIVE))
#define AS_NATIVE(value)        ((ObjNative*) AS_OBJ(value))

#define IS_UPVALUE(value)       (is_obj_type(value, OBJ_UPVALUE))
#define AS_UPVALUE(value)       ((ObjUpvalue*) AS_OBJ(value))

#define IS_CLOSURE(value)       (is_obj_type(value, OBJ_CLOSURE))
#define AS_CLOSURE(value)       ((ObjClosure*) AS_OBJ(value))

#define IS_CLASS(value)         (is_obj_type(value, OBJ_CLASS))
#define AS_CLASS(value)         ((ObjClass*) AS_OBJ(value))

#define IS_INSTANCE(value)      (is_obj_type(value, OBJ_INSTANCE))
#define AS_INSTANCE(value)      ((ObjInstance*) AS_OBJ(value))

#define IS_BOUND_METHOD(value)  (is_obj_type(value, OBJ_BOUND_METHOD))
#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*) AS_OBJ(value))

#define STRING_MAX_LEN          0x7FFFFF00


// inlined for fast-path
inline static bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

Obj* alloc_object(size_t size, ObjType type);
void free_object(Obj* object);
void mark_object(Obj* object);

Value string_value(VM* vm, const char* str, int length);
Value concatenate_strings(VM* vm, Value a, Value b);

ObjFunction* new_function(VM* vm);
Value define_native(VM* vm, const char* name, NativeFn fn);

ObjClosure* new_closure(VM* vm, ObjFunction* fn);
ObjUpvalue* new_upvalue(VM* vm, Value* value);
ObjClass* new_class(VM* vm, ObjString* name);
ObjInstance* new_instance(VM* vm, ObjClass* klass);
ObjBoundMethod* new_bound_method(VM* vm, Value receiver, Value method);
