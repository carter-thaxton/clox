#include "compiler.h"
#include "parser.h"
#include "chunk.h"
#include "vm.h"
#include <stdlib.h>     // strtod

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif


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
    PREC_PRIMARY
};

typedef void (*ParseFn)();

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

// forward declarations
static void grouping();
static void unary();
static void binary();
static void number();
static void literal();
static void string();
static void variable();

static ParseRule rules[] = {
    [TOKEN_EOF]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]           = {NULL,     NULL,   PREC_NONE},

    [TOKEN_LEFT_PAREN]      = {grouping, NULL,   PREC_NONE},
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

    [TOKEN_AND]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]             = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]              = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]             = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]              = {NULL,     NULL,   PREC_NONE},
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
static Chunk *compiling_chunk;
static VM *compiling_vm;


static Chunk* current_chunk() {
    return compiling_chunk;
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
    if (index >= MAX_CONSTANTS) {
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

static void emit_return(int line) {
    emit_byte(OP_RETURN, line);
}

static void end_compiler() {
    emit_return(parser.line());

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error()) {
        print_chunk(current_chunk(), "code");
    }
    #endif
}

// create and register a string object, and add string as a constant value to chunk
// return the constant index, or -1 on failure
static int make_identifier_constant(Token* token) {
    Value value = string_value(compiling_vm, token->start, token->length);
    if (IS_NIL(value)) { parser.error("String too long."); return -1; }

    int index = current_chunk()->add_constant_value(value);
    if (index >= MAX_CONSTANTS) {
        parser.error("Too many constants in one chunk.");
        return -1;
    }

    return index;
}

// parse identifier as variable name, make string object, and add as a constant value to chunk
// return the constant index on success
// return -1 and produce parser error on failure
static int parse_variable(const char* err_msg) {
    if (parser.consume(TOKEN_IDENTIFIER, err_msg)) {
        return make_identifier_constant(&parser.previous);
    } else {
        return -1;
    }
}


//
// Expressions
//

static ParseRule* get_rule(TokenType op_type) {
    return &rules[op_type];
}

static void parse_precedence(Precedence precedence) {
    parser.advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (!prefix_rule) return parser.error("Expect expression.");
    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        parser.advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        if (!infix_rule) return parser.error("missing infix function");
        infix_rule();
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void literal() {
    int line = parser.line();
    TokenType op_type = parser.previous.type;

    switch (op_type) {
        case TOKEN_NIL:     emit_byte(OP_NIL, line); break;
        case TOKEN_TRUE:    emit_byte(OP_TRUE, line); break;
        case TOKEN_FALSE:   emit_byte(OP_FALSE, line); break;

        default: return parser.error("unreachable literal");
    }
}

static void string() {
    const char* str = parser.previous.start + 1;    // skip opening "
    int length = parser.previous.length - 2;        // without opening and closing ""
    Value val = string_value(compiling_vm, str, length);
    if (IS_NIL(val)) return parser.error("String too long.");
    emit_constant(val);
}

static void grouping() {
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary() {
    int line = parser.line();
    TokenType op_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (op_type) {
        case TOKEN_MINUS:   emit_byte(OP_NEGATE, line); break;
        case TOKEN_BANG:    emit_byte(OP_NOT, line); break;
        case TOKEN_PLUS:    break;  // NOP

        default: return parser.error("unreachable unary operator");
    }
}

static void binary() {
    int line = parser.line();
    TokenType op_type = parser.previous.type;
    ParseRule* rule = get_rule(op_type);

    Precedence next_prec = (Precedence) (rule->precedence + 1);  // left-associative
    parse_precedence(next_prec);

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

static void variable() {
    int line = parser.line();
    int constant = make_identifier_constant(&parser.previous);
    emit_get_global(constant, line);
}

//
// Statements
//

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

static void statement() {
    if (parser.match(TOKEN_PRINT)) {
        print_stmt();
    } else {
        expression_stmt();
    }
}

static void var_decl() {
    int global = parse_variable("Expect variable name.");
    if (global < 0) return;
    int line = parser.line();

    if (parser.match(TOKEN_EQUAL)) {
        // parse initial value, leaving it on stack
        expression();
    } else {
        // use nil as initial value
        emit_byte(OP_NIL, parser.line());
    }

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    // TODO: handle local variables
    emit_define_global(global, line);
}

static void declaration() {
    if (parser.match(TOKEN_VAR)) {
        var_decl();
    } else {
        statement();
    }

    parser.synchronize();
}


//
// Top-Level Interface
//

bool compile(const char* src, Chunk* chunk, VM* vm) {
    // use static globals, so not re-entrant
    parser.init(src);
    compiling_chunk = chunk;
    compiling_vm = vm;

    while (!parser.match(TOKEN_EOF)) {
        declaration();
    }

    end_compiler();

    bool ok = !parser.had_error();

    // clear the static globals
    parser.init("");
    compiling_chunk = NULL;
    compiling_vm = NULL;

    return ok;
}
