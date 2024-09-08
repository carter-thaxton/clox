#include "parser.h"
#include <stdio.h>

Parser::Parser() {
}

Parser::~Parser() {
}

void Parser::init(const char* src) {
    this->lexer.init(src);
    this->error_count = 0;
    this->panic_mode = false;
    advance();
}

void Parser::error_at_current(const char* msg) {
    error_at(&current, msg);
}

void Parser::error(const char* msg) {
    error_at(&previous, msg);
}

void Parser::error_at(Token* token, const char* msg) {
    if (panic_mode) return;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        // print only 'length' chars from 'start'
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", msg);

    panic_mode = true;
    error_count++;
}

bool Parser::had_error() {
    return error_count > 0;
}

void Parser::advance() {
    previous = current;

    // move to next token, and report but skip any errors
    while (true) {
        current = lexer.next_token();

        if (current.type != TOKEN_ERROR)
            break;

        error_at_current(current.start);
    }
}

void Parser::consume(TokenType type, const char* msg) {
    if (current.type == type) {
        advance();
        return;
    }

    error_at_current(msg);
}
