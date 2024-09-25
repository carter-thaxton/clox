#include "debug.h"
#include "object.h"

#include <stdio.h>

static int print_simple_inst(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static void print_constant(const char* name, Chunk* chunk, int constant) {
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
}

static void print_invoke(const char* name, Chunk* chunk, int constant, int argc) {
    printf("%-16s (%d args) %4d '", name, argc, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
}

static void print_index(const char* name, Chunk* chunk, int index) {
    printf("%-16s %4d\n", name, index);
}

static int print_constant_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    print_constant(name, chunk, constant);
    return offset + 2;
}

static int print_constant_16_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    print_constant(name, chunk, constant);
    return offset + 3;
}

static int print_constant_24_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;
    print_constant(name, chunk, constant);
    return offset + 4;
}

static int print_index_inst(const char* name, Chunk* chunk, int offset) {
    int index = chunk->code[offset + 1];
    print_index(name, chunk, index);
    return offset + 2;
}

static int print_index_16_inst(const char* name, Chunk* chunk, int offset) {
    int index = chunk->code[offset + 1];
    index |= chunk->code[offset + 2] << 8;
    print_index(name, chunk, index);
    return offset + 3;
}

static int print_index_24_inst(const char* name, Chunk* chunk, int offset) {
    int index = chunk->code[offset + 1];
    index |= chunk->code[offset + 2] << 8;
    index |= chunk->code[offset + 3] << 16;
    print_index(name, chunk, index);
    return offset + 4;
}

static int print_signed_16_inst(const char* name, Chunk* chunk, int offset) {
    int16_t index = chunk->code[offset + 1];
    index |= chunk->code[offset + 2] << 8;
    print_index(name, chunk, (int) index);
    return offset + 3;
}

static int print_upvalue_refs(Chunk* chunk, int constant, int offset) {
    ObjFunction* fn = AS_FUNCTION(chunk->constants.values[constant]);
    for (int i=0; i < fn->upvalue_count; i++) {
        int index = chunk->code[offset++];
        index |= chunk->code[offset++] << 8;
        bool is_local = (index & 0x8000) != 0;
        index &= 0x7FFF;
        printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upval", index);
    }
    return offset;
}

static int print_closure_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    print_constant(name, chunk, constant);
    return print_upvalue_refs(chunk, constant, offset + 2);
}

static int print_closure_16_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    print_constant(name, chunk, constant);
    return print_upvalue_refs(chunk, constant, offset + 3);
}

static int print_closure_24_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;
    print_constant(name, chunk, constant);
    return print_upvalue_refs(chunk, constant, offset + 4);
}

static int print_invoke_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    int argc = chunk->code[offset + 2];
    print_invoke(name, chunk, constant, argc);
    return offset + 3;
}

static int print_invoke_16_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    int argc = chunk->code[offset + 3];
    print_invoke(name, chunk, constant, argc);
    return offset + 4;
}

static int print_invoke_24_inst(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1];
    constant |= chunk->code[offset + 2] << 8;
    constant |= chunk->code[offset + 3] << 16;
    int argc = chunk->code[offset + 4];
    print_invoke(name, chunk, constant, argc);
    return offset + 5;
}

void print_chunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->length; ) {
        offset = print_instruction(chunk, offset);
    }
}

int print_instruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset-1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t inst = chunk->code[offset];

    switch (inst) {
    case OP_NIL:
        return print_simple_inst("OP_NIL", offset);
    case OP_FALSE:
        return print_simple_inst("OP_FALSE", offset);
    case OP_TRUE:
        return print_simple_inst("OP_TRUE", offset);

    case OP_CONSTANT:
        return print_constant_inst("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_16:
        return print_constant_16_inst("OP_CONSTANT_16", chunk, offset);
    case OP_CONSTANT_24:
        return print_constant_24_inst("OP_CONSTANT_24", chunk, offset);

    case OP_CLASS:
        return print_constant_inst("OP_CLASS", chunk, offset);
    case OP_CLASS_16:
        return print_constant_16_inst("OP_CLASS_16", chunk, offset);
    case OP_CLASS_24:
        return print_constant_24_inst("OP_CLASS_24", chunk, offset);

    case OP_METHOD:
        return print_constant_inst("OP_METHOD", chunk, offset);
    case OP_METHOD_16:
        return print_constant_16_inst("OP_METHOD_16", chunk, offset);
    case OP_METHOD_24:
        return print_constant_24_inst("OP_METHOD_24", chunk, offset);

    case OP_INVOKE:
        return print_invoke_inst("OP_INVOKE", chunk, offset);
    case OP_INVOKE_16:
        return print_invoke_16_inst("OP_INVOKE_16", chunk, offset);
    case OP_INVOKE_24:
        return print_invoke_24_inst("OP_INVOKE_24", chunk, offset);

    case OP_CLOSURE:
        return print_closure_inst("OP_CLOSURE", chunk, offset);
    case OP_CLOSURE_16:
        return print_closure_16_inst("OP_CLOSURE_16", chunk, offset);
    case OP_CLOSURE_24:
        return print_closure_24_inst("OP_CLOSURE_24", chunk, offset);

    case OP_DEFINE_GLOBAL:
        return print_constant_inst("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL_16:
        return print_constant_16_inst("OP_DEFINE_GLOBAL_16", chunk, offset);
    case OP_DEFINE_GLOBAL_24:
        return print_constant_24_inst("OP_DEFINE_GLOBAL_24", chunk, offset);

    case OP_GET_GLOBAL:
        return print_constant_inst("OP_GET_GLOBAL", chunk, offset);
    case OP_GET_GLOBAL_16:
        return print_constant_16_inst("OP_GET_GLOBAL_16", chunk, offset);
    case OP_GET_GLOBAL_24:
        return print_constant_24_inst("OP_GET_GLOBAL_24", chunk, offset);

    case OP_SET_GLOBAL:
        return print_constant_inst("OP_SET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL_16:
        return print_constant_16_inst("OP_SET_GLOBAL_16", chunk, offset);
    case OP_SET_GLOBAL_24:
        return print_constant_24_inst("OP_SET_GLOBAL_24", chunk, offset);

    case OP_GET_LOCAL:
        return print_index_inst("OP_GET_LOCAL", chunk, offset);
    case OP_GET_LOCAL_16:
        return print_index_16_inst("OP_GET_LOCAL_16", chunk, offset);
    case OP_GET_LOCAL_24:
        return print_index_24_inst("OP_GET_LOCAL_24", chunk, offset);

    case OP_SET_LOCAL:
        return print_index_inst("OP_SET_LOCAL", chunk, offset);
    case OP_SET_LOCAL_16:
        return print_index_16_inst("OP_SET_LOCAL_16", chunk, offset);
    case OP_SET_LOCAL_24:
        return print_index_24_inst("OP_SET_LOCAL_24", chunk, offset);

    case OP_GET_UPVALUE:
        return print_index_inst("OP_GET_UPVALUE", chunk, offset);
    case OP_GET_UPVALUE_16:
        return print_index_16_inst("OP_GET_UPVALUE_16", chunk, offset);
    case OP_GET_UPVALUE_24:
        return print_index_24_inst("OP_GET_UPVALUE_24", chunk, offset);

    case OP_SET_UPVALUE:
        return print_index_inst("OP_SET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE_16:
        return print_index_16_inst("OP_SET_UPVALUE_16", chunk, offset);
    case OP_SET_UPVALUE_24:
        return print_index_24_inst("OP_SET_UPVALUE_24", chunk, offset);

    case OP_GET_PROPERTY:
        return print_constant_inst("OP_GET_PROPERTY", chunk, offset);
    case OP_GET_PROPERTY_16:
        return print_constant_16_inst("OP_GET_PROPERTY_16", chunk, offset);
    case OP_GET_PROPERTY_24:
        return print_constant_24_inst("OP_GET_PROPERTY_24", chunk, offset);

    case OP_SET_PROPERTY:
        return print_constant_inst("OP_SET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY_16:
        return print_constant_16_inst("OP_SET_PROPERTY_16", chunk, offset);
    case OP_SET_PROPERTY_24:
        return print_constant_24_inst("OP_SET_PROPERTY_24", chunk, offset);

    case OP_ADD:
        return print_simple_inst("OP_ADD", offset);
    case OP_SUBTRACT:
        return print_simple_inst("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
        return print_simple_inst("OP_MULTIPLY", offset);
    case OP_DIVIDE:
        return print_simple_inst("OP_DIVIDE", offset);
    case OP_EQUAL:
        return print_simple_inst("OP_EQUAL", offset);
    case OP_LESS:
        return print_simple_inst("OP_LESS", offset);
    case OP_GREATER:
        return print_simple_inst("OP_GREATER", offset);

    case OP_NEGATE:
        return print_simple_inst("OP_NEGATE", offset);
    case OP_NOT:
        return print_simple_inst("OP_NOT", offset);

    case OP_POP:
        return print_simple_inst("OP_POP", offset);
    case OP_POPN:
        return print_index_inst("OP_POPN", chunk, offset);
    case OP_PRINT:
        return print_simple_inst("OP_PRINT", offset);
    case OP_RETURN:
        return print_simple_inst("OP_RETURN", offset);
    case OP_JUMP:
        return print_signed_16_inst("OP_JUMP", chunk, offset);
    case OP_JUMP_IF_FALSE:
        return print_signed_16_inst("OP_JUMP_IF_FALSE", chunk, offset);
    case OP_JUMP_IF_TRUE:
        return print_signed_16_inst("OP_JUMP_IF_TRUE", chunk, offset);
    case OP_CALL:
        return print_index_inst("OP_CALL", chunk, offset);
    case OP_CLOSE_UPVALUE:
        return print_simple_inst("OP_CLOSE_UPVALUE", offset);

    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}

void print_value(Value value) {
    switch (value.type) {
        case VAL_NIL: {
            printf("nil");
            return;
        }
        case VAL_BOOL: {
            printf(AS_BOOL(value) ? "true" : "false");
            return;
        }
        case VAL_NUMBER: {
            printf("%g", AS_NUMBER(value));
            return;
        }
        case VAL_OBJ: {
            print_object(AS_OBJ(value));
            return;
        }
    }

    printf("Unrecognized value type\n");
}

void print_value_array(ValueArray* array) {
    for (int i = 0; i < array->length; i++) {
        printf(" %3d: ", i);
        print_value(array->values[i]);
        printf("\n");
    }
}


void print_object(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            printf("%s", ((ObjString*) object)->chars);
            return;
        }
        case OBJ_FUNCTION: {
            ObjString* name = ((ObjFunction*) object)->name;
            if (name) {
                printf("<fn %s>", name->chars);
            } else {
                printf("<script>");
            }
            return;
        }
        case OBJ_NATIVE: {
            printf("<native fn>");
            return;
        }
        case OBJ_UPVALUE: {
            ObjUpvalue* upvalue = ((ObjUpvalue*) object);
            printf("<upvalue ");
            print_value(*upvalue->location);
            printf(">");
            return;
        }
        case OBJ_CLOSURE: {
            ObjString* name = ((ObjClosure*) object)->fn->name;
            printf("<fn %s closure>", name->chars);
            return;
        }
        case OBJ_CLASS: {
            ObjString* name = ((ObjClass*) object)->name;
            printf("%s", name->chars);
            return;
        }
        case OBJ_INSTANCE: {
            ObjString* name = ((ObjInstance*) object)->klass->name;
            printf("%s instance", name->chars);
            return;
        }
        case OBJ_BOUND_METHOD: {
            Value method = ((ObjBoundMethod*) object)->method;
            print_value(method);
            return;
        }
    }
}

void print_table(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;  // also skip tombstones

        printf("    %10s = ", entry->key->chars);
        print_value(entry->value);
        printf("\n");
    }
}

void print_strings(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;  // also skip tombstones

        printf("  %s", entry->key->chars);
        if (i % 32 == 31) {
            printf("\n");
        }
    }
}
