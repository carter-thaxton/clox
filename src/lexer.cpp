#include "lexer.h"
#include <string.h>

Lexer::Lexer(const char* src) {
    this->start = src;
    this->current = src;
    this->line = 1;
}

Lexer::~Lexer() {
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return  (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c == '_');
}

Token Lexer::next_token() {
    skip_whitespace();

    this->start = this->current;

    if (at_eof())
        return make_token(TOKEN_EOF);

    char c = advance();

    if (is_alpha(c))
        return identifier();

    if (is_digit(c))
        return number();

    switch (c) {
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '-': return make_token(TOKEN_MINUS);
        case '+': return make_token(TOKEN_PLUS);
        case ';': return make_token(TOKEN_SEMICOLON);
        case '/': return make_token(TOKEN_SLASH);
        case '*': return make_token(TOKEN_STAR);

        case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

        case '"': return string();
    }

    return error_token("Unexpected character.");
}

Token Lexer::make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = this->start;
    token.length = (int) (this->current - this->start);
    token.line = this->line;
    return token;
}

Token Lexer::error_token(const char* msg) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = msg;
    token.length = strlen(msg);
    token.line = this->line;
    return token;
}

bool Lexer::at_eof() {
    return this->current == '\0';
}

char Lexer::peek() {
    return *this->current;
}

char Lexer::peek_next() {
    if (at_eof()) return '\0';
    return this->current[1];
}

char Lexer::advance() {
    return *this->current++;
}

bool Lexer::match(char expected) {
    if (at_eof()) return false;
    if (peek() != expected) return false;
    this->current++;
    return true;
}

void Lexer::skip_whitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;

            case '\n':
                this->line++;
                advance();
                break;

            case '/':
                if (peek_next() == '/') {
                    // skip comment until end of line
                    while (!at_eof() && peek() != '\n') {
                        advance();
                    }
                } else {
                    // just a slash
                    return;
                }
                break;

            default:
                return;
        }
    }
}

Token Lexer::string() {
    while (!at_eof() && peek() != '"') {
        if (peek() == '\n')
            this->line++;
        advance();
    }

    if (at_eof())
        return error_token("Unterminated string.");

    advance(); // closing quote

    return make_token(TOKEN_STRING);
}

Token Lexer::number() {
    // 123
    while (is_digit(peek()))
        advance();

    // .456
    if (peek() == '.' && is_digit(peek_next())) {
        advance(); // dot

        while (is_digit(peek()))
            advance();
    }

    return make_token(TOKEN_NUMBER);
}

Token Lexer::identifier() {
    while (is_alpha(peek()) || is_digit(peek())) {
        advance();
    }

    switch (this->start[0]) {
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f': {
            if (this->current - this->start > 1) {
                switch (this->start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        }
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't': {
            if (this->current - this->start > 1) {
                switch (this->start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        }
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }

    return make_token(TOKEN_IDENTIFIER);
}

Token Lexer::check_keyword(int start, int length, const char* rest, TokenType type) {
    if ((this->current - this->start == start + length) &&
        (memcmp(this->start + start, rest, length) == 0)) {
        return make_token(type);
    } else {
        return make_token(TOKEN_IDENTIFIER);
    }
}

