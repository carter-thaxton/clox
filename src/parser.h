#pragma once

#include "common.h"
#include "lexer.h"

struct Parser {
    Parser();
    ~Parser();

    void init(const char* src);
    void advance();
    void consume(TokenType type, const char* msg);

    void error(const char* msg);  // at previous
    void error_at_current(const char* msg);
    void error_at(Token* token, const char* msg);
    bool had_error();

    Lexer lexer;
    Token current;
    Token previous;
    int error_count;
    bool panic_mode;
};
