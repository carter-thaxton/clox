#include "value.h"
#include "memory.h"
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


static Obj* alloc_obj(size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static ObjString* copy_string(const char* str, int length) {
    // <-- ObjString -->
    // [ type | length | chars ... ]
    size_t size = sizeof(ObjString) + length + 1;
    ObjString* result = (ObjString*) alloc_obj(size, OBJ_STRING);
    memcpy(result->chars, str, length);
    result->chars[length] = '\0';
    result->length = length;
    return result;
}

Value make_string(const char* str, int length) {
    ObjString *obj = copy_string(str, length);
    return OBJ_VAL(obj);
}
