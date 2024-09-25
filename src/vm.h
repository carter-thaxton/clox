#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAME_MAX 64
#define STACK_MAX 65536

enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
};

struct CallFrame {
    ObjFunction* fn;
    ObjClosure* closure;
    uint8_t* ip;
    Value* values;
};

class VM {
public:
    VM();
    ~VM();

    InterpretResult interpret(ObjFunction* main_fn);

    void register_object(Obj* object);
    void gc();

    // for debugging
    void set_debug_mode(bool debug) { this->debug_mode = debug; }
    bool is_debug_mode() { return debug_mode; }
    int get_object_count() { return object_count; }
    int get_string_count() { return strings.get_count(); }
    int get_string_capacity() { return strings.get_capacity(); }
    Table* get_strings() { return &strings; }
    Table* get_globals() { return &globals; }
    void clear();

private:
    void reset_stack();
    void free_all_objects();
    void mark_objects();
    int sweep_objects();

    InterpretResult runtime_error(const char* format, ...);

    // some fast-path inlined functions
    CallFrame* frame();
    Chunk* chunk();

    uint8_t read_byte();
    int read_unsigned_16();
    int read_unsigned_24();
    int read_signed_16();

    int read_unsigned(int length);
    Value read_constant(int length);

    Value peek(int depth);
    void push(Value value);
    Value pop();
    void pop_n(int n);

    void closure(Value fn);
    ObjUpvalue* capture_upvalue(int index);
    void close_upvalues(Value* value);
    void define_method(ObjString* name);
    bool bind_method(ObjClass* klass, ObjString* name);
    bool get_property(ObjString* name);
    bool set_property(ObjString* name);

    InterpretResult invoke(ObjString* name, int argc);
    InterpretResult invoke_from_class(ObjClass* klass, ObjString* name, int argc);

    InterpretResult call_function(ObjFunction* fn, int argc);
    InterpretResult call_closure(ObjClosure* closure, int argc);
    InterpretResult call_class(ObjClass* klass, int argc);
    InterpretResult call_bound_method(ObjBoundMethod* bound, int argc);
    InterpretResult call_value(Value callee, int argc);

    InterpretResult run();

    CallFrame frames[FRAME_MAX];
    int frame_count;
    Obj* objects;
    int object_count;
    int gc_object_threshold;
    ObjUpvalue* open_upvalues;
    Table strings;
    Table globals;
    Value stack[STACK_MAX];
    Value* stack_top;
    bool debug_mode;
    ObjString* init_string;

    friend Value string_value(VM* vm, const char* str, int length);
    friend Value concatenate_strings(VM* vm, Value a, Value b);
    friend Value define_native(VM* vm, const char* name, NativeFn fn);
};
