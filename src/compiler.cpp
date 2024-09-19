#include "compiler.h"
#include "parser.h"
#include "chunk.h"
#include "vm.h"

#include <stdlib.h>     // strtod
#include <string.h>     // memcmp
#include <assert.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define MAX_BREAK_STMTS     64
#define MAX_LOCALS          2048

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
    int local_count;
    int scope_depth;
    Local locals[MAX_LOCALS];
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
static void and_(bool lvalue);
static void or_(bool lvalue);
static void call(bool lvalue);


static ParseRule rules[] = {
    [TOKEN_EOF]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]           = {NULL,     NULL,   PREC_NONE},

    [TOKEN_LEFT_PAREN]      = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]             = {NULL,     NULL,   PREC_NONE},
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
    [TOKEN_SUPER]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]            = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]           = {NULL,     NULL,   PREC_NONE},
};

// global variables, to avoid passing through all functions below
// initialized and then cleared all within compile(), so it shouldn't leak
static Parser parser;
static VM *compiling_vm;
static Compiler* current;

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
    int index = current_chunk()->write_constant_value(value, parser.line());
    if (index > MAX_INDEX) {
        parser.error("Too many constants in one chunk.");
        return -1;
    }
    return index;
}

static void emit_define_global(int constant, int line) {
    current_chunk()->write_define_global(constant, line);
}

static void emit_get_global(int constant, int line) {
    current_chunk()->write_get_global(constant, line);
}

static void emit_set_global(int constant, int line) {
    current_chunk()->write_set_global(constant, line);
}

static void emit_get_local(int index, int line) {
    current_chunk()->write_get_local(index, line);
}

static void emit_set_local(int index, int line) {
    current_chunk()->write_set_local(index, line);
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

// compare two identifier tokens
static bool identifiers_equal(Token* token1, Token* token2) {
    if (token1->type != TOKEN_IDENTIFIER) return false;
    if (token2->type != TOKEN_IDENTIFIER) return false;
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

// lookup a local variable by name
// return a non-negative local index on success, and -1 if not found
// produces error when attempting to resolve a declared but undefined variable.  still returns local index
static int resolve_local(Token* name) {
    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (identifiers_equal(&local->name, name)) {
            if (local->depth < 0) {
                parser.error("Can't read local variable in its own initializer.");
            }
            return i;
        }
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
// return the local or global constant index on success
// return -1 and produce parser error on failure
static int parse_variable(const char* err_msg) {
    if (!parser.consume(TOKEN_IDENTIFIER, err_msg)) return -1;

    if (!declare_variable()) return -1;

    if (current->scope_depth > 0) {
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
static int pop_scope_to(int scope_depth, int line) {
    int locals_to_pop = 0;
    int l = current->local_count;
    while (l > 0 && current->locals[l-1].depth > scope_depth) {
        locals_to_pop++;
        l--;
    }

    emit_pop_count(locals_to_pop, line);
    return locals_to_pop;
}

static void end_scope() {
    assert(current->scope_depth > 0);
    current->scope_depth--;
    current->local_count -= pop_scope_to(current->scope_depth, parser.line());
}


static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->parent = current;
    compiler->fn = new_function(compiling_vm);
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;

    // define an initial local variable for the function itself
    Local* local = &compiler->locals[compiler->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
    local->name.line = 0;
    local->name.type = TOKEN_FUN;

    switch (type) {
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

    current = compiler;
}

static ObjFunction* end_compiler() {
    // emit return nil, unless previous instruction was already an explicit return
    bool prev_return = current_chunk()->length > 0 && current_chunk()->read_back(0) == OP_RETURN;
    if (!prev_return) {
        emit_bytes(OP_NIL, OP_RETURN, parser.line());
    }

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error()) {
        const char* name = current->fn->name ? current->fn->name->chars : "<script>";
        print_chunk(current_chunk(), name);
    }
    #endif

    ObjFunction* result = current->fn;

    current = current->parent;
    return result;
}


//
// Expressions
//

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

static void variable(bool lvalue) {
    int line = parser.line();

    int index = resolve_local(&parser.previous);
    if (index >= 0) {
        if (lvalue && parser.match(TOKEN_EQUAL)) {
            expression();
            emit_set_local(index, line);
        } else {
            emit_get_local(index, line);
        }
    } else {
        int constant = make_identifier_constant(&parser.previous);
        if (lvalue && parser.match(TOKEN_EQUAL)) {
            expression();
            emit_set_global(constant, line);
        } else {
            emit_get_global(constant, line);
        }
    }
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
                parser.error("Can't have more than 255 arguments.");
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
    int arg_count = arguments();
    emit_bytes(OP_CALL, arg_count, line);
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
    pop_scope_to(loop_ctx->scope_depth, line);

    // jump to loop end, keeping track of jump to patch later
    int jump_exit = emit_jump(OP_JUMP, line);
    loop_ctx->break_stmts[loop_ctx->num_break_stmts++] = jump_exit;

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");
}

static void continue_stmt(LoopContext* loop_ctx) {
    if (loop_ctx == NULL)
        return parser.error("Can only continue within loop.");

    int line = parser.line();
    pop_scope_to(loop_ctx->scope_depth, line);

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
        emit_bytes(OP_NIL, OP_RETURN, line);
    } else {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN, line);
    }
}

static void block(LoopContext* loop_ctx) {
    while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF)) {
        declaration(loop_ctx);
    }
    parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
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

static void fun_decl() {
    int index = parse_variable("Expect function name.");
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
    int index = parse_variable("Expect variable name.");
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
    if (parser.match(TOKEN_FUN)) {
        fun_decl();
    } else if (parser.match(TOKEN_VAR)) {
        var_decl();
    } else {
        statement(loop_ctx);
    }

    parser.synchronize();
}

static void function_helper(FunctionType type) {
    const char* msg;
    switch (type) {
        case TYPE_ANONYMOUS:
            msg = "Expect '(' after fun.";
            break;
        case TYPE_SCRIPT:
            msg = "Internal error.  TYPE_SCRIPT should not be parsed with function_helper()";
            break;
        case TYPE_FUNCTION:
            msg = "Expect '(' after function name.";
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
            int index = parse_variable("Expect parameter name.");
            if (index < 0) break;
        } while (parser.match(TOKEN_COMMA));
    }

    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    block(NULL);

    // no need for end_scope()
    ObjFunction* result = end_compiler();

    emit_constant(OBJ_VAL(result));
}


//
// Top-Level Interface
//

ObjFunction* compile(const char* src, VM* vm) {
    // use static globals, so not re-entrant
    parser.init(src);
    compiling_vm = vm;
    current = NULL;

    Compiler compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    while (!parser.match(TOKEN_EOF)) {
        declaration(NULL);
    }

    ObjFunction* result = end_compiler();
    bool ok = !parser.had_error();

    // clear the static globals
    current = NULL;
    compiling_vm = NULL;
    parser.init("");

    return ok ? result : NULL;
}
