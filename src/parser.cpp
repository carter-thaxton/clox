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

//
// Tokens
//

void Parser::advance() {
    previous = current;

    // move to next token non-error token
    while (true) {
        current = lexer.next_token();
        if (current.type == TOKEN_ERROR) {
            error_at_current(current.start);
        } else {
            break;
        }
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

bool Parser::check(TokenType type) const {
    return current.type == type;
}


//
// Errors
//

void Parser::error(const char* msg) {
    error_at(&previous, msg);
}

void Parser::error_at_current(const char* msg) {
    error_at(&current, msg);
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

bool Parser::synchronize() {
    if (!panic_mode) return false;
    panic_mode = false;

    while (current.type != TOKEN_EOF) {
        if (previous.type == TOKEN_SEMICOLON) return true;
        switch (current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return true;

            default:
                advance();
        }
    }

    return true;
}

