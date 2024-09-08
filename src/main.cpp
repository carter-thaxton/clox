
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

#define EX_USAGE    (64)    // incorrect command-line usage
#define EX_DATAERR  (65)    // lexer and parser errors
#define EX_NOINPUT  (66)    // invalid input file
#define EX_SOFTWARE (70)    // runtime errors
#define EX_IOERR    (74)    // I/O error


InterpretResult interpret(const char* src) {
    return INTERPRET_OK;
}

void repl() {
    char line[1024];

    while (true) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(EX_IOERR);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*) malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(EX_IOERR);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(EX_IOERR);
    }
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

void run_file(const char* path) {
    char *file = read_file(path);
    int result = interpret(file);
    free(file);

    if (result == INTERPRET_COMPILE_ERROR) exit(EX_DATAERR);
    if (result == INTERPRET_RUNTIME_ERROR) exit(EX_SOFTWARE);
}

int main(int argc, const char* argv[]) {
    Chunk chunk;

    chunk.write_constant(1.2, 123);
    chunk.write_constant(3.4, 123);
    chunk.write(OP_ADD, 123);
    chunk.write_constant(5.6, 123);
    chunk.write(OP_DIVIDE, 123);
    chunk.write(OP_NEGATE, 123);
    chunk.write(OP_RETURN, 123);

    // print_chunk(&chunk, "test chunk");

    VM vm;

    for (int i=0; i < 100000000; i++) {
        InterpretResult result = vm.interpret(&chunk);
    }
    // printf("result: %d\n", result);

    return 0;

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s [path]\n", argv[0]);
        exit(EX_USAGE);
    }

    return 0;
}

