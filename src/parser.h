#pragma once

#include "common.h"
#include "lexer.h"

struct Parser {
    Parser();
    ~Parser();

    void init(const char* src);
    void advance();
    void consume(TokenType type, const char* msg);
    bool match(TokenType type);
    bool check(TokenType type) const;

    void error(const char* msg);  // commonly at previous
    void error_at_current(const char* msg);
    void error_at(Token* token, const char* msg);
    bool synchronize();

    int line() const { return previous.line; }  // commonly at previous
    int line_at_current() const { return current.line; }

    bool had_error() const { return error_count > 0; }

    // publicly readable
    Token current;
    Token previous;

private:
    Lexer lexer;
    int error_count;
    bool panic_mode;
};
