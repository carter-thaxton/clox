#include "compiler.h"
#include "lexer.h"

#include <stdio.h>

void compile(const char* src) {
    Lexer lexer(src);

    int line = -1;
    while (true) {
        Token token = lexer.next_token();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, token.length, token.start);

        if (token.type == TOKEN_EOF) break;
    }
}
