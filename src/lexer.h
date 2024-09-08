#pragma once

#include "common.h"

enum TokenType {
    TOKEN_EOF,
    TOKEN_ERROR,

    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,

    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,

    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
};

struct Token {
    TokenType type;
    const char* start;
    int length;
    int line;
};

class Lexer {
public:
    Lexer(const char* src);
    ~Lexer();

    Token next_token();
    bool at_eof();

private:
    Token make_token(TokenType type);
    Token error_token(const char* msg);
    char peek();
    char peek_next();
    char advance();
    bool match(char expected);
    void skip_whitespace();
    Token string();
    Token number();
    Token identifier();
    Token check_keyword(int start, int length, const char* rest, TokenType type);

    const char* start;
    const char* current;
    int line;
    int token_line;
};
