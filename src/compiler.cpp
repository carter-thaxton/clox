#include "compiler.h"
#include "parser.h"
#include <stdlib.h>

static Parser parser;
static Chunk *compiling_chunk;


static Chunk* current_chunk() {
    return compiling_chunk;
}

static void emit_byte(uint8_t byte) {
    current_chunk()->write(byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_constant(Value value) {
    int index = current_chunk()->write_constant(value, parser.previous.line);
    if (index >= MAX_CONSTANTS) {
        parser.error("Too many constants in one chunk.");
    }
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

static void end_compiler() {
    emit_return();
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

bool compile(const char* src, Chunk* chunk) {
    parser.init(src);
    compiling_chunk = chunk;

    // expression();
    parser.consume(TOKEN_EOF, "Expect end of expression.");
    end_compiler();

    return !parser.had_error();
}
