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

int Parser::line() {
    return previous.line;
}

int Parser::line_at_current() {
    return current.line;
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
    if (check(type)) {
        advance();
    } else {
        error_at_current(msg);
    }
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    } else {
        return false;
    }
}

bool Parser::check(TokenType type) {
    return current.type == type;
}

void Parser::synchronize() {
    panic_mode = false;

    while (current.type != TOKEN_EOF) {
        if (previous.type == TOKEN_SEMICOLON) return;
        switch (current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:
                ; // Do nothing.
        }

        advance();
    }
}

