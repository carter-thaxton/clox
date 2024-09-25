#include "object.h"
#include "memory.h"
#include "vm.h"
#include "debug.h"
#include <string.h>
#include <new>
#include <assert.h>

Obj* alloc_object(size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(NULL, 0, size);
    object->type = type;
    object->next = NULL;  // will be set when registered with VM
    object->marked = false;
    return object;
}

void free_object(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) object;
            size_t size = sizeof(ObjString) + (string->length + 1) * sizeof(char);
            reallocate(string, size, 0);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*) object;
            fn->chunk.~Chunk();
            FREE(ObjFunction, fn);
            break;
        }
        case OBJ_NATIVE: {
            ObjNative* fn = (ObjNative*) object;
            FREE(ObjNative, fn);
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*) object;
            FREE(ObjUpvalue, upvalue);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            size_t size = sizeof(ObjClosure) + closure->upvalue_count * sizeof(ObjUpvalue*);
            reallocate(closure, size, 0);
            break;
        }
    }
}

static uint32_t hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t) key[i];
        hash *= 16777619;
    }
    return hash;
}

Value string_value(VM* vm, const char* str, int length) {
    if (length >= STRING_MAX_LEN) return NIL_VAL;

    // first check if we have interned this string
    uint32_t hash = hash_string(str, length);
    ObjString* interned = vm->strings.find_string(str, length, hash);
    if (interned) return OBJ_VAL(interned);

    // <-- ObjString -->
    // [ type | length | chars ... ]
    size_t size = sizeof(ObjString) + (length + 1) * sizeof(char);
    ObjString* result = (ObjString*) alloc_object(size, OBJ_STRING);
    memcpy(result->chars, str, length);
    result->chars[length] = '\0';
    result->length = length;
    result->hash = hash;

    // keep track of string for interning and garbage collection
    vm->register_object((Obj*) result);
    vm->strings.insert(result, NIL_VAL);

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
    vm->register_object((Obj*) result);
    vm->strings.insert(result, NIL_VAL);

    return OBJ_VAL(result);
}

ObjFunction* new_function(VM* vm) {
    ObjFunction* result = (ObjFunction*) alloc_object(sizeof(ObjFunction), OBJ_FUNCTION);

    result->name = NULL;
    result->arity = 0;
    new (&result->chunk) Chunk();

    vm->register_object((Obj*) result);

    return result;
}

Value define_native(VM* vm, const char* name, NativeFn fn) {
    ObjNative* fn_obj = (ObjNative*) alloc_object(sizeof(ObjNative), OBJ_NATIVE);
    fn_obj->native_fn = fn;
    vm->register_object((Obj*) fn_obj);

    // push/pop for GC
    Value fn_val = OBJ_VAL(fn_obj);
    vm->push(fn_val);

    Value name_val = string_value(vm, name, strlen(name));
    vm->push(name_val);

    vm->globals.insert(AS_STRING(name_val), fn_val);

    vm->pop();
    vm->pop();

    return fn_val;
}

ObjClosure* new_closure(VM* vm, ObjFunction* fn) {
    size_t size = sizeof(ObjClosure) + fn->upvalue_count * sizeof(ObjUpvalue*);
    ObjClosure* result = (ObjClosure*) alloc_object(size, OBJ_CLOSURE);

    result->fn = fn;
    result->upvalue_count = fn->upvalue_count;
    for (int i=0; i < fn->upvalue_count; i++) {
        result->upvalues[i] = NULL;
    }

    vm->register_object((Obj*) result);

    return result;
}

ObjUpvalue* new_upvalue(VM* vm, Value* value) {
    ObjUpvalue* result = (ObjUpvalue*) alloc_object(sizeof(ObjUpvalue), OBJ_UPVALUE);

    result->closed = NIL_VAL;
    result->location = value;
    result->next = NULL;

    vm->register_object((Obj*) result);

    return result;
}

void mark_object(Obj* object) {
    if (object == NULL) return;
    // assert(object != NULL);
    if (object->marked) return;

    #ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*) object);
    print_value(OBJ_VAL(object));
    printf("\n");
    #endif

    object->marked = true;

    switch (object->type) {
        case OBJ_STRING:
        case OBJ_NATIVE:
            break;

        case OBJ_FUNCTION: {
            ObjFunction* fn = (ObjFunction*) object;
            mark_object((Obj*) fn->name);
            fn->chunk.constants.mark_objects();
            break;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = (ObjUpvalue*) object;
            mark_value(upvalue->closed);
            // don't follow list of open upvalues - VM will do that
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            mark_object((Obj*) closure->fn);
            for (int i=0; i < closure->upvalue_count; i++) {
                mark_object((Obj*) closure->upvalues[i]);
            }
            break;
        }
    }
}
