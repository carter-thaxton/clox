#include "compiler.h"
#include "parser.h"
#include "chunk.h"
#include "vm.h"

#include <stdlib.h>     // strtod
#include <string.h>     // memcmp
#include <assert.h>

#include "debug.h"

#define MAX_BREAK_STMTS     64
#define MAX_LOCALS          2048    // architecture limits these to 32767
#define MAX_UPVALUES        2048    // (two bytes, with one bit used to distinguish between local vs upvalue)

enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . ()
    PREC_PRIMARY,
};

enum FunctionType {
    TYPE_SCRIPT,
    TYPE_ANONYMOUS,
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_INITIALIZER,
};

typedef void (*ParseFn)(bool lvalue);

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

struct Local {
    Token name;
    int depth;
    bool is_captured;
};

struct Upvalue {
    int index;
    bool is_local;
};

struct LoopContext {
    int scope_depth;
    int loop_start;
    int num_break_stmts;
    int break_stmts[MAX_BREAK_STMTS];
};

struct Compiler {
    Compiler* parent;
    ObjFunction *fn;
    FunctionType type;
    int scope_depth;
    int local_count;
    int upvalue_count;
    Local locals[MAX_LOCALS];
    Upvalue upvalues[MAX_UPVALUES];
};

struct ClassCompiler {
  ClassCompiler* parent;
  bool has_superclass;
};

// forward declarations
static void grouping(bool lvalue);
static void unary(bool lvalue);
static void binary(bool lvalue);
static void number(bool lvalue);
static void literal(bool lvalue);
static void string(bool lvalue);
static void variable(bool lvalue);
static void function(bool lvalue);
static void this_(bool lavalue);
static void super_(bool lvalue);
static void and_(bool lvalue);
static void or_(bool lvalue);
static void call(bool lvalue);
static void dot(bool lvalue);


static ParseRule rules[] = {
    [TOKEN_EOF]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]           = {NULL,     NULL,   PREC_NONE},

    [TOKEN_LEFT_PAREN]      = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]             = {NULL,     dot,    PREC_CALL},
    [TOKEN_MINUS]           = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]            = {unary,    binary, PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]           = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]            = {NULL,     binary, PREC_FACTOR},

    [TOKEN_BANG]            = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,     binary, PREC_COMPARISON},

    [TOKEN_IDENTIFIER]      = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]          = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]          = {number,   NULL,   PREC_NONE},

    [TOKEN_AND]             = {NULL,     and_,   PREC_AND},
    [TOKEN_BREAK]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CONTINUE]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]             = {function, NULL,   PREC_NONE},
    [TOKEN_IF]              = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]             = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]              = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]           = {super_,   NULL,   PREC_NONE},
    [TOKEN_THIS]            = {this_,    NULL,   PREC_NONE},
    [TOKEN_TRUE]            = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]           = {NULL,     NULL,   PREC_NONE},
};

// global variables, to avoid passing through all functions below
// initialized and then cleared all within compile(), so it shouldn't leak
static Parser parser;
static VM *compiling_vm;
static Compiler* current;
static ClassCompiler* current_class;

// synthetic tokens for 'this' and 'super'
static Token this_token  = { TOKEN_THIS,  "this",  4, 0 };
static Token super_token = { TOKEN_SUPER, "super", 5, 0 };

static Chunk* current_chunk() {
    return &current->fn->chunk;
}

static int here() {
    return current_chunk()->length;
}

static void emit_byte(uint8_t byte, int line) {
    current_chunk()->write(byte, line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2, int line) {
    emit_byte(byte1, line);
    emit_byte(byte2, line);
}

static int emit_constant(Value value) {
    int index = current_chunk()->add_constant_value(value);
    if (index > MAX_INDEX) {
        parser.error("Too many constants in one chunk.");
        return -1;
    }
    current_chunk()->write_variable_length_opcode(OP_CONSTANT, index, parser.line());
    return index;
}

static int emit_closure(Value value) {
    assert(IS_FUNCTION(value));
    int index = current_chunk()->add_constant_value(value);
    if (index > MAX_INDEX) {
        parser.error("Too many constants in one chunk.");
        return -1;
    }
    current_chunk()->write_variable_length_opcode(OP_CLOSURE, index, parser.line());
    return index;
}

static void emit_upvalue_ref(int index, bool is_local, int line) {
    // encode as 15-bit index, with high bit indicating local vs upvalue
    uint16_t word = index;
    if (is_local) word |= 0x8000;
    emit_bytes(word & 0xFF, word >> 8, line);
}

static void emit_class(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_CLASS, constant, line);
}

static void emit_method(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_METHOD, constant, line);
}

static void emit_invoke(int constant, int argc, int line) {
    current_chunk()->write_variable_length_opcode(OP_INVOKE, constant, line);
    emit_byte(argc, line);
}

static void emit_define_global(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_DEFINE_GLOBAL, constant, line);
}

static void emit_get_global(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_GET_GLOBAL, constant, line);
}

static void emit_set_global(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_SET_GLOBAL, constant, line);
}

static void emit_get_local(int index, int line) {
    current_chunk()->write_variable_length_opcode(OP_GET_LOCAL, index, line);
}

static void emit_set_local(int index, int line) {
    current_chunk()->write_variable_length_opcode(OP_SET_LOCAL, index, line);
}

static void emit_get_upvalue(int index, int line) {
    current_chunk()->write_variable_length_opcode(OP_GET_UPVALUE, index, line);
}

static void emit_set_upvalue(int index, int line) {
    current_chunk()->write_variable_length_opcode(OP_SET_UPVALUE, index, line);
}

static void emit_get_property(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_GET_PROPERTY, constant, line);
}

static void emit_set_property(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_SET_PROPERTY, constant, line);
}

static void emit_get_super(int constant, int line) {
    current_chunk()->write_variable_length_opcode(OP_GET_SUPER, constant, line);
}

static int emit_jump(uint8_t opcode, int line) {
    emit_byte(opcode, line);
    emit_bytes(0xFF, 0xFF, line);           // 2 bytes for placeholder
    return here();                          // index of byte just after placeholder, provide to patch_jump() later
}

static void patch_jump(int placeholder_index, int to_index) {
    // calculate relative jump distance, from original placeholder to current instruction
    int jump = to_index - placeholder_index;

    // result must be in range -32678 to 32767
    if (jump > INT16_MAX) {
        return parser.error("Too much code to jump over.");
    } else if (jump < INT16_MIN) {
        return parser.error("Loop body too large.");
    }

    uint8_t* code = current_chunk()->code;

    // placeholder_index points to just after a big-endian 16-bit value
    code[placeholder_index-2] = jump & 0xFF;
    code[placeholder_index-1] = (jump >> 8) & 0xFF;
}

// emit OP_POP and OP_POPN instructions
static void emit_pop_count(int count, int line) {
    assert(count >= 0);

    while (count > 1) {
        int n = count <= 255 ? count : 255;
        emit_byte(OP_POPN, line);
        emit_byte(n, line);
        count -= n;
    }

    if (count > 0) {
        assert(count == 1);
        emit_byte(OP_POP, line);
    }
}

static void emit_return(int line) {
    bool prev_return = current_chunk()->length > 0 && current_chunk()->read_back(0) == OP_RETURN;

    if (current->type == TYPE_INITIALIZER) {
        // always return 'this' from initializer, stored as local 0
        emit_bytes(OP_GET_LOCAL, 0, line);
        emit_byte(OP_RETURN, line);
    } else if (prev_return) {
        // optimized away - previous instruction was already an explicit return
    } else {
        // return nil
        emit_bytes(OP_NIL, OP_RETURN, line);
    }
}

// create and register a string object, and add string as a constant value to chunk
// return the constant index, or -1 on failure
static int make_identifier_constant(Token* token) {
    Value value = string_value(compiling_vm, token->start, token->length);
    if (IS_NIL(value)) { parser.error("String too long."); return -1; }

    int index = current_chunk()->add_constant_value(value);
    if (index > MAX_INDEX) {
        parser.error("Too many constants in one chunk.");
        return -1;
    }

    return index;
}

// compare two tokens representing identifiers
// note that token may not be of type TOKEN_IDENTIFIER, e.g. allow comparison of TOKEN_THIS or TOkEN_SUPER
static bool identifiers_equal(Token* token1, Token* token2) {
    if (token1->length != token2->length) return false;
    return memcmp(token1->start, token2->start, token1->length) == 0;
}

// returns true on success, false with error on too many locals
static bool declare_local(Token* token) {
    if (current->local_count >= MAX_LOCALS) {
        parser.error("Too many local variables in function.");
        return false;
    }

    Local* local = &current->locals[current->local_count++];
    local->name = *token;
    local->depth = -1;  // declare only - set to scope_depth later in define_local()
    local->is_captured = false;
    return true;
}

// assertion error if called without corresponding declare_local()
static void define_local(int index) {
    assert(current->local_count > 0);
    assert(index >= 0);
    assert(index < current->local_count);
    Local* local = &current->locals[index];
    assert(local->depth < 0);
    local->depth = current->scope_depth;
}

// define an upvalue reference to a variable at given index
// pass is_local true if it's in the immediately enclosing scope
// checks for duplicates.  returns upvalue index
static int define_upvalue(Compiler* compiler, int var_index, bool is_local) {
    if (compiler->upvalue_count >= MAX_UPVALUES) {
        parser.error("Too many closure variables in function.");
        return -1;
    }

    // check if we've already defined an upvalue referring to this variable, and return its index
    int upvalue_count = compiler->upvalue_count;
    for (int i=0; i < upvalue_count; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == var_index && upvalue->is_local == is_local)
            return i;
    }

    Upvalue* upvalue = &compiler->upvalues[upvalue_count];
    upvalue->index = var_index;
    upvalue->is_local = is_local;

    compiler->upvalue_count++;
    return upvalue_count;
}

// lookup a local variable by name
// return a non-negative local index on success, and -1 if not found
// produces error when attempting to resolve a declared but undefined variable.  still returns local index
static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiers_equal(&local->name, name)) {
            if (local->depth < 0) {
                parser.error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

// recursively lookup an upvalue variable by name
// return a non-negative upvalue index on success, and -1 if not found as available upvalue
static int resolve_upvalue(Compiler* compiler, Token* name) {
    if (compiler->parent == NULL) return -1;

    // immediate parent scope
    int index = resolve_local(compiler->parent, name);
    if (index >= 0) {
        compiler->parent->locals[index].is_captured = true;
        return define_upvalue(compiler, index, true);  // references local
    }

    // recurse
    index = resolve_upvalue(compiler->parent, name);
    if (index >= 0) {
        return define_upvalue(compiler, index, false); // intermedite upvalue
    }

    return -1;
}

// returns true on success, false with error
static bool declare_variable() {
    // nothing to do in global scope, and always allowed to redeclare a new global
    if (current->scope_depth <= 0) return true;

    Token* name = &parser.previous;

    // check if already declared in local scope
    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth >= 0 && local->depth < current->scope_depth) {
            // reached outer scope, ok to declare
            break;
        }

        if (identifiers_equal(&local->name, name)) {
            parser.error("Already a variable with this name in this scope.");
            return false;
        }
    }

    return declare_local(name);
}

// parse identifier as variable name
// in local scope, checks and registers as local variable
// in global scope, makes string object, and adds as a constant value to chunk
// set always_make_constant to behave like global scope and create an identifier constant
// return the local or global constant index on success
// return -1 and produce parser error on failure
static int parse_variable(const char* err_msg, bool always_make_constant) {
    if (!parser.consume(TOKEN_IDENTIFIER, err_msg)) return -1;

    if (!declare_variable()) return -1;

    if (!always_make_constant && current->scope_depth > 0) {
        // local: index is the most recently declared variable above
        return current->local_count - 1;
    } else {
        // global: index is to a constant for variable name
        return make_identifier_constant(&parser.previous);
    }
}

static void begin_scope() {
    current->scope_depth++;
}

// emit OP_POP and OP_POPN instructions to pop locals to given scope depth
// used to break out of loop, without actually updating scope_depth or local_count
//
// when capture_locals is true, emit OP_CLOSE_UPVALUE instead of OP_POP for locals
// that have been marked with is_captured
static int pop_scope_to(int scope_depth, int line, bool capture_locals) {
    int l = current->local_count;
    int total_locals_to_pop = 0;
    int locals_to_pop = 0;

    while (l > 0 && current->locals[l-1].depth > scope_depth) {
        if (capture_locals && current->locals[l-1].is_captured) {
            if (locals_to_pop > 0) {
                emit_pop_count(locals_to_pop, line);
                locals_to_pop = 0;
            }
            emit_byte(OP_CLOSE_UPVALUE, line);
        } else {
            locals_to_pop++;
        }

        total_locals_to_pop++;
        l--;
    }

    if (locals_to_pop > 0) {
        emit_pop_count(locals_to_pop, line);
    }

    return total_locals_to_pop;
}

static void end_scope() {
    assert(current->scope_depth > 0);
    current->scope_depth--;
    current->local_count -= pop_scope_to(current->scope_depth, parser.line(), true);
}


static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->parent = current;
    current = compiler;

    compiler->fn = new_function(compiling_vm);
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;

    // reserve an initial local variable for 'this' or the function itself
    Local* local = &compiler->locals[compiler->local_count++];
    if (type == TYPE_METHOD || type == TYPE_INITIALIZER) {
        local->name.start = "this";
        local->name.length = 4;
        local->name.type = TOKEN_THIS;
    } else {
        local->name.start = "";
        local->name.length = 0;
        local->name.type = TOKEN_FUN;
    }
    local->name.line = 0;
    local->depth = 0;
    local->is_captured = false;

    switch (type) {
        case TYPE_METHOD:
        case TYPE_FUNCTION: {
            // use previous token as function name
            Value fn_name = string_value(compiling_vm, parser.previous.start, parser.previous.length);
            compiler->fn->name = AS_STRING(fn_name);
            break;
        }
        case TYPE_ANONYMOUS: {
            // use "" as function name
            Value fn_name = string_value(compiling_vm, "", 0);
            compiler->fn->name = AS_STRING(fn_name);
            break;
        }
        default: {
            // leave function name as nil
        }
    }
}

static ObjFunction* end_compiler() {
    emit_return(parser.line());

    if (compiling_vm->is_debug_mode() && !parser.had_error()) {
        const char* name = current->fn->name ? current->fn->name->chars : "<script>";
        print_chunk(current_chunk(), name);
    }

    ObjFunction* result = current->fn;
    result->upvalue_count = current->upvalue_count;

    current = current->parent;

    return result;
}


//
// Expressions
//

static void variable_helper(Token* name, bool lvalue);
static void function_helper(FunctionType type);

static ParseRule* get_rule(TokenType op_type) {
    return &rules[op_type];
}

static void expr_precedence(Precedence precedence) {
    bool lvalue = precedence <= PREC_ASSIGNMENT;

    parser.advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (!prefix_rule) return parser.error("Expect expression.");
    prefix_rule(lvalue);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        parser.advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        if (!infix_rule) return parser.error("missing infix function");
        infix_rule(lvalue);
    }

    if (lvalue && parser.match(TOKEN_EQUAL)) {
        parser.error("Invalid assignment target.");
    }
}

static void expression() {
    expr_precedence(PREC_ASSIGNMENT);
}

static void number(bool _lvalue) {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void literal(bool _lvalue) {
    int line = parser.line();
    TokenType op_type = parser.previous.type;

    switch (op_type) {
        case TOKEN_NIL:     emit_byte(OP_NIL, line); break;
        case TOKEN_FALSE:   emit_byte(OP_FALSE, line); break;
        case TOKEN_TRUE:    emit_byte(OP_TRUE, line); break;

        default: return parser.error("unreachable literal");
    }
}

static void string(bool _lvalue) {
    const char* str = parser.previous.start + 1;    // skip opening "
    int length = parser.previous.length - 2;        // without opening and closing ""
    Value val = string_value(compiling_vm, str, length);
    if (IS_NIL(val)) return parser.error("String too long.");
    emit_constant(val);
}

static void grouping(bool _lvalue) {
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool _lvalue) {
    int line = parser.line();
    TokenType op_type = parser.previous.type;

    expr_precedence(PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS:   emit_byte(OP_NEGATE, line); break;
        case TOKEN_BANG:    emit_byte(OP_NOT, line); break;
        case TOKEN_PLUS:    break;  // NOP

        default: return parser.error("unreachable unary operator");
    }
}

static void binary(bool _lvalue) {
    int line = parser.line();
    TokenType op_type = parser.previous.type;
    ParseRule* rule = get_rule(op_type);

    Precedence next_prec = (Precedence) (rule->precedence + 1);  // left-associative
    expr_precedence(next_prec);

    switch (op_type) {
        case TOKEN_PLUS:    emit_byte(OP_ADD, line); break;
        case TOKEN_MINUS:   emit_byte(OP_SUBTRACT, line); break;
        case TOKEN_STAR:    emit_byte(OP_MULTIPLY, line); break;
        case TOKEN_SLASH:   emit_byte(OP_DIVIDE, line); break;

        case TOKEN_BANG_EQUAL:      emit_bytes(OP_EQUAL, OP_NOT, line); break;
        case TOKEN_EQUAL_EQUAL:     emit_byte(OP_EQUAL, line); break;
        case TOKEN_LESS:            emit_byte(OP_LESS, line); break;
        case TOKEN_LESS_EQUAL:      emit_bytes(OP_GREATER, OP_NOT, line); break;
        case TOKEN_GREATER:         emit_byte(OP_GREATER, line); break;
        case TOKEN_GREATER_EQUAL:   emit_bytes(OP_LESS, OP_NOT, line); break;

        default: return parser.error("unreachable binary operator");
    }
}

static void this_(bool lvalue) {
    if (current_class == NULL) {
        return parser.error("Can't use 'this' outside of a class.");
    }
    variable_helper(&parser.previous, false);  // disallow assignment to 'this'
}

static void super_(bool lvalue) {
    if (current_class == NULL) {
        return parser.error("Can't use 'super' outside of a class.");
    } else if (!current_class->has_superclass) {
        return parser.error("Can't use 'super' in a class with no superclass.");
    }

    int line = parser.line();
    parser.consume(TOKEN_DOT, "Expect '.' after 'super'.");
    parser.consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    int method_constant = make_identifier_constant(&parser.previous);

    variable_helper(&this_token, false);
    variable_helper(&super_token, false);

    emit_get_super(method_constant, line);
}

static void variable(bool lvalue) {
    variable_helper(&parser.previous, lvalue);
}

static void function(bool _lvalue) {
    function_helper(TYPE_ANONYMOUS);
}

static void and_(bool _lvalue) {
    int line = parser.line();
    int jump = emit_jump(OP_JUMP_IF_FALSE, line);

    emit_byte(OP_POP, line);
    expr_precedence(PREC_AND);

    patch_jump(jump, here());
}

static void or_(bool _lvalue) {
    int line = parser.line();
    int jump = emit_jump(OP_JUMP_IF_TRUE, line);

    emit_byte(OP_POP, line);
    expr_precedence(PREC_OR);

    patch_jump(jump, here());
}

static int arguments() {
    int count = 0;
    if (!parser.check(TOKEN_RIGHT_PAREN)) {
        do {
            if (count >= 255) {
                parser.error_at_current("Can't have more than 255 arguments.");
                break;
            }
            expression();
            count++;
        } while (parser.match(TOKEN_COMMA));
    }
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return count;
}

static void call(bool _lvalue) {
    int line = parser.line();
    int argc = arguments();
    emit_bytes(OP_CALL, argc, line);
}

static void dot(bool lvalue) {
    int line = parser.line();
    parser.consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    int name_constant = make_identifier_constant(&parser.previous);

    if (lvalue && parser.match(TOKEN_EQUAL)) {
        expression();
        emit_set_property(name_constant, line);
    } else if (parser.match(TOKEN_LEFT_PAREN)) {
        int argc = arguments();
        emit_invoke(name_constant, argc, line);
    } else {
        emit_get_property(name_constant, line);
    }
}


//
// Statements
//

static void declaration(LoopContext* loop_ctx);
static void statement(LoopContext* loop_ctx);
static void var_decl();

static void expression_stmt() {
    int line = parser.line();
    expression();
    parser.consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP, line);
}

static void print_stmt() {
    int line = parser.line();
    expression();
    parser.consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT, line);
}

static void if_stmt(LoopContext* loop_ctx) {
    int if_line = parser.line();
    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int then_jump = emit_jump(OP_JUMP_IF_FALSE, if_line);
    emit_byte(OP_POP, if_line);
    statement(loop_ctx);

    int else_line = parser.line_at_current();
    int else_jump = emit_jump(OP_JUMP, else_line);
    patch_jump(then_jump, here());
    emit_byte(OP_POP, else_line);

    if (parser.match(TOKEN_ELSE)) {
        statement(loop_ctx);
    }

    patch_jump(else_jump, here());
}

static void while_stmt() {
    int line = parser.line();
    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");

    // new loop context
    LoopContext loop_ctx;
    loop_ctx.loop_start = here();
    loop_ctx.scope_depth = current->scope_depth;
    loop_ctx.num_break_stmts = 0;

    // loop condition
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    int jump_exit = emit_jump(OP_JUMP_IF_FALSE, line);

    // loop body
    emit_byte(OP_POP, line);
    statement(&loop_ctx);

    // loop back to start
    int jump_loop = emit_jump(OP_JUMP, line);
    patch_jump(jump_loop, loop_ctx.loop_start);

    // exit
    patch_jump(jump_exit, here());
    emit_byte(OP_POP, line); // pop loop condition

    // patch any 'break' statements
    for (int i=0; i < loop_ctx.num_break_stmts; i++) {
        patch_jump(loop_ctx.break_stmts[i], here());
    }
}

static void for_stmt() {
    int line = parser.line();
    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    begin_scope();

    // initializer
    if (parser.match(TOKEN_SEMICOLON)) {
        // none
    } else if (parser.match(TOKEN_VAR)) {
        var_decl();
    } else {
        expression_stmt();
    }

    // new loop context
    LoopContext loop_ctx;
    loop_ctx.loop_start = here();
    loop_ctx.scope_depth = current->scope_depth;
    loop_ctx.num_break_stmts = 0;

    int jump_exit = -1;

    // loop condition
    if (parser.match(TOKEN_SEMICOLON)) {
        // none
    } else {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        jump_exit = emit_jump(OP_JUMP_IF_FALSE, line);
        emit_byte(OP_POP, line);
    }

    // increment
    if (parser.match(TOKEN_RIGHT_PAREN)) {
        // none
        // after body, loop to start
    } else {
        // jump to body first
        int jump_body = emit_jump(OP_JUMP, line);

        // then back to increment
        int inc_start = here();
        expression();
        emit_byte(OP_POP, line);

        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        int jump_loop = emit_jump(OP_JUMP, line);
        patch_jump(jump_loop, loop_ctx.loop_start);

        // after body, loop to increment
        loop_ctx.loop_start = inc_start;

        // and before increment, jump to body
        patch_jump(jump_body, here());
    }

    // loop body
    statement(&loop_ctx);

    // loop back to start or increment
    int jump_loop = emit_jump(OP_JUMP, line);
    patch_jump(jump_loop, loop_ctx.loop_start);

    // exit
    if (jump_exit >= 0) {
        patch_jump(jump_exit, here());
        emit_byte(OP_POP, line); // pop loop condition
    }

    // patch any 'break' statements
    for (int i=0; i < loop_ctx.num_break_stmts; i++) {
        patch_jump(loop_ctx.break_stmts[i], here());
    }

    end_scope();
}

static void break_stmt(LoopContext* loop_ctx) {
    if (loop_ctx == NULL)
        return parser.error("Can only break within loop.");

    if (loop_ctx->num_break_stmts >= MAX_BREAK_STMTS)
        return parser.error("Too many break statements in one loop.");

    int line = parser.line();
    pop_scope_to(loop_ctx->scope_depth, line, false);

    // jump to loop end, keeping track of jump to patch later
    int jump_exit = emit_jump(OP_JUMP, line);
    loop_ctx->break_stmts[loop_ctx->num_break_stmts++] = jump_exit;

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");
}

static void continue_stmt(LoopContext* loop_ctx) {
    if (loop_ctx == NULL)
        return parser.error("Can only continue within loop.");

    int line = parser.line();
    pop_scope_to(loop_ctx->scope_depth, line, false);

    // loop back to start
    int jump_loop = emit_jump(OP_JUMP, line);
    patch_jump(jump_loop, loop_ctx->loop_start);

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
}

static void return_stmt(LoopContext* loop_ctx) {
    if (current->type == TYPE_SCRIPT) {
        return parser.error("Can't return from top-level code.");
    }

    int line = parser.line();
    if (parser.match(TOKEN_SEMICOLON)) {
        emit_return(line);
    } else {
        if (current->type == TYPE_INITIALIZER) {
            return parser.error("Can't return a value from an initializer.");
        }
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN, line);
    }
}

static void block(LoopContext* loop_ctx) {
    while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF)) {
        declaration(loop_ctx);
    }

    if (!parser.error_at_end()) {
        parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    }
}

static void statement(LoopContext* loop_ctx) {
    if (parser.match(TOKEN_SEMICOLON)) {
        // empty statement
    } else if (parser.match(TOKEN_PRINT)) {
        print_stmt();
    } else if (parser.match(TOKEN_IF)) {
        if_stmt(loop_ctx);
    } else if (parser.match(TOKEN_WHILE)) {
        while_stmt();
    } else if (parser.match(TOKEN_FOR)) {
        for_stmt();
    } else if (parser.match(TOKEN_BREAK)) {
        break_stmt(loop_ctx);
    } else if (parser.match(TOKEN_CONTINUE)) {
        continue_stmt(loop_ctx);
    } else if (parser.match(TOKEN_RETURN)) {
        return_stmt(loop_ctx);
    } else if (parser.match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block(loop_ctx);
        end_scope();
    } else {
        expression_stmt();
    }
}

static void method() {
    int line = parser.line();
    parser.consume(TOKEN_IDENTIFIER, "Expect method name.");
    int name_constant = make_identifier_constant(&parser.previous);

    bool is_init = (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0);
    function_helper(is_init ? TYPE_INITIALIZER : TYPE_METHOD);

    emit_method(name_constant, line);
}

static void class_decl() {
    int name_constant = parse_variable("Expect class name.", true);
    if (name_constant < 0) return;

    Token name_token = parser.previous;
    int line = parser.line();

    if (current->scope_depth > 0) {
        // mark initialized, so class can refer to itself by name
        define_local(current->local_count - 1);
    }

    emit_class(name_constant, line);

    if (current->scope_depth == 0) {
        emit_define_global(name_constant, line);
    }

    // new scope for class compiler
    ClassCompiler class_compiler;
    class_compiler.parent = current_class;
    class_compiler.has_superclass = false;
    current_class = &class_compiler;

    if (parser.match(TOKEN_LESS)) {
        class_compiler.has_superclass = true;

        parser.consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        if (identifiers_equal(&name_token, &parser.previous)) {
            return parser.error("A class can't inherit from itself.");
        }

        // put superclass on stack, start new scope, and define 'super' to refer to it
        variable_helper(&parser.previous, false);
        begin_scope();
        declare_local(&super_token);
        define_local(current->local_count - 1);

        variable_helper(&name_token, false);        // put class on stack
        emit_byte(OP_INHERIT, parser.line());
    }

    variable_helper(&name_token, false);  // put class on stack

    parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF)) {
        method();
    }

    if (!parser.error_at_end()) {
        parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
        emit_byte(OP_POP, line);
    }

    // pop class scope
    if (class_compiler.has_superclass) {
        end_scope();
    }
    current_class = class_compiler.parent;
}

static void fun_decl() {
    int index = parse_variable("Expect function name.", false);
    if (index < 0) return;

    int line = parser.line();
    if (current->scope_depth > 0) {
        // mark initialized, so function can refer to itself by name
        define_local(index);
    }

    function_helper(TYPE_FUNCTION);

    if (current->scope_depth == 0) {
        emit_define_global(index, line);
    }
}

static void var_decl() {
    int index = parse_variable("Expect variable name.", false);
    if (index < 0) return;

    int line = parser.line();

    if (parser.match(TOKEN_EQUAL)) {
        // parse initial value, leaving it on stack
        expression();
    } else {
        // use nil as initial value
        emit_byte(OP_NIL, line);
    }

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    if (current->scope_depth > 0) {
        define_local(index);
    } else {
        emit_define_global(index, line);
    }
}

static void declaration(LoopContext* loop_ctx) {
    if (parser.match(TOKEN_CLASS)) {
        class_decl();
    } else if (parser.match(TOKEN_FUN)) {
        fun_decl();
    } else if (parser.match(TOKEN_VAR)) {
        var_decl();
    } else {
        statement(loop_ctx);
    }

    parser.synchronize();
}

static void variable_helper(Token* name, bool lvalue) {
    int line = parser.line();

    // local
    int local = resolve_local(current, name);
    if (local >= 0) {
        if (lvalue && parser.match(TOKEN_EQUAL)) {
            expression();
            emit_set_local(local, line);
        } else {
            emit_get_local(local, line);
        }
        return;
    }

    // upvalue
    int upvalue = resolve_upvalue(current, name);
    if (upvalue >= 0) {
        if (lvalue && parser.match(TOKEN_EQUAL)) {
            expression();
            emit_set_upvalue(upvalue, line);
        } else {
            emit_get_upvalue(upvalue, line);
        }
        return;
    }

    // global
    int constant = make_identifier_constant(name);
    if (lvalue && parser.match(TOKEN_EQUAL)) {
        expression();
        emit_set_global(constant, line);
    } else {
        emit_get_global(constant, line);
    }
}

static void function_helper(FunctionType type) {
    const char* msg;
    switch (type) {
        case TYPE_SCRIPT:
            assert(!"TYPE_SCRIPT should not be parsed with function_helper");
            break;
        case TYPE_ANONYMOUS:
            if (parser.check(TOKEN_IDENTIFIER)) {
                // function declaration where only anonymous expression is allowed
                return parser.error("Expect expression.");
            }
            msg = "Expect '(' after fun.";
            break;
        case TYPE_FUNCTION:
            msg = "Expect '(' after function name.";
            break;
        case TYPE_METHOD:
        case TYPE_INITIALIZER:
            msg = "Expect '(' after method name.";
            break;
    }

    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    parser.consume(TOKEN_LEFT_PAREN, msg);

    // parse params
    if (!parser.check(TOKEN_RIGHT_PAREN)) {
        do {
            if (current->fn->arity >= 255) {
                parser.error_at_current("Can't have more than 255 parameters.");
                break;
            }
            current->fn->arity++;
            int index = parse_variable("Expect parameter name.", false);
            if (index < 0) break;
            define_local(index);
        } while (parser.match(TOKEN_COMMA));
    }

    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    block(NULL);

    // no need for end_scope()
    ObjFunction* fn = end_compiler();

    // only create closure when necessary
    if (fn->upvalue_count > 0) {
        // closure with dedicated instruction, followed by variable number of upvalue references
        emit_closure(OBJ_VAL(fn));
        int line = parser.line();
        for (int i=0; i < fn->upvalue_count; i++) {
            emit_upvalue_ref(compiler.upvalues[i].index, compiler.upvalues[i].is_local, line);
        }
    } else {
        // function as constant
        emit_constant(OBJ_VAL(fn));
    }
}


//
// Top-Level Interface
//

ObjFunction* compile(const char* src, VM* vm) {
    // use static globals, so not re-entrant
    parser.init(src);
    compiling_vm = vm;
    current = NULL;
    current_class = NULL;

    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    while (!parser.match(TOKEN_EOF)) {
        declaration(NULL);
    }

    ObjFunction* result = end_compiler();
    bool ok = !parser.had_error();

    // clear the static globals
    current_class = NULL;
    current = NULL;
    compiling_vm = NULL;
    parser.init("");

    return ok ? result : NULL;
}

void mark_compiler_roots() {
    Compiler* compiler = current;
    while (compiler) {
        mark_object((Obj*) compiler->fn);
        compiler = compiler->parent;
    }
}
