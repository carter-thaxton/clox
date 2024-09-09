#include "value.h"
#include "memory.h"
#include "vm.h"
#include <string.h>

ValueArray::ValueArray() {
    this->values = NULL;
    this->capacity = 0;
    this->length = 0;
}

ValueArray::~ValueArray() {
    FREE_ARRAY(Value, this->values, this->capacity);

    this->values = NULL;
    this->capacity = 0;
    this->length = 0;
}

void ValueArray::write(Value value) {
    if (this->capacity < this->length + 1) {
        int old_capacity = this->capacity;
        int new_capacity = GROW_CAPACITY(old_capacity);
        this->values = GROW_ARRAY(Value, this->values, old_capacity, new_capacity);
        this->capacity = new_capacity;
    }

    this->values[this->length] = value;
    this->length++;
}

static uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NIL: return true;
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: {
            if (OBJ_TYPE(a) != OBJ_TYPE(b)) return false;
            switch (OBJ_TYPE(a)) {
                case OBJ_STRING: {
                    ObjString* sa = AS_STRING(a);
                    ObjString* sb = AS_STRING(b);
                    if (sa->length != sb->length) return false;
                    return memcmp(sa->chars, sb->chars, sa->length) == 0;
                }
            }
        }
    }
    return false;
}

Value string_value(VM* vm, const char* str, int length) {
    if (length >= STRING_MAX_LEN) return NIL_VAL;

    // first check if we have interned this string
    uint32_t hash = hash_string(str, length);
    ObjString* interned = vm->strings.find_string(str, length, hash);
    if (interned) return OBJ_VAL(interned);

    // <-- ObjString -->
    // [ type | length | chars ... ]
    size_t size = sizeof(ObjString) + length + 1;
    ObjString* result = (ObjString*) alloc_object(size, OBJ_STRING);
    memcpy(result->chars, str, length);
    result->chars[length] = '\0';
    result->length = length;
    result->hash = hash;

    // keep track of string for interning and garbage collection
    vm->strings.insert(result, NIL_VAL);
    vm->register_object((Obj*) result);

    return OBJ_VAL(result);
}

Value concatenate_strings(VM* vm, Value a, Value b) {
    ObjString* sa = AS_STRING(a);
    ObjString* sb = AS_STRING(b);

    int length = sa->length + sb->length;
    if (length >= STRING_MAX_LEN) return NIL_VAL;

    // create string object as concatenation
    size_t size = sizeof(ObjString) + length + 1;
    ObjString* result = (ObjString*) alloc_object(size, OBJ_STRING);
    memcpy(result->chars, sa->chars, sa->length);
    memcpy(result->chars + sa->length, sb->chars, sb->length);
    result->chars[length] = '\0';
    result->length = length;
    result->hash = hash_string(result->chars, length);

    // now check if we have already interned the resulting string
    ObjString* interned = vm->strings.find_string(result->chars, result->length, result->hash);
    if (interned) {
        // alas, we've already made this string
        free_object((Obj*) result);
        return OBJ_VAL(interned);
    }

    // keep track of string for interning and garbage collection
    vm->strings.insert(result, NIL_VAL);
    vm->register_object((Obj*) result);

    return OBJ_VAL(result);
}

Obj* alloc_object(size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(NULL, 0, size);
    object->type = type;
    object->next = NULL;  // will be set when registered with VM
    return object;
}

void free_object(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) object;
            size_t size = sizeof(ObjString) + string->length + 1;
            reallocate(string, size, 0);
            break;
        }
    }
}
