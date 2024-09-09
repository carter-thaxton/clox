#include "compiler.h"
#include "parser.h"
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

    [TOKEN_IDENTIFIER]      = {NULL,     NULL,   PREC_NONE},
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

static void emit_constant(Value value) {
    int index = current_chunk()->write_constant(value, parser.previous.line);
    if (index >= MAX_CONSTANTS) {
        parser.error("Too many constants in one chunk.");
    }
}

static void emit_return(int line) {
    emit_byte(OP_RETURN, line);
}

static void end_compiler() {
    emit_return(parser.previous.line);

    #ifdef DEBUG_PRINT_CODE
    if (!parser.had_error()) {
        print_chunk(current_chunk(), "code");
    }
    #endif
}


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
    int line = parser.previous.line;
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
    int line = parser.previous.line;
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
    int line = parser.previous.line;
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


bool compile(const char* src, Chunk* chunk, VM* vm) {
    parser.init(src);
    compiling_chunk = chunk;
    compiling_vm = vm;

    expression();
    parser.consume(TOKEN_EOF, "Expect end of expression.");
    end_compiler();

    return !parser.had_error();
}
