
#include "common.h"
#include "chunk.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#define EX_USAGE    (64)    // incorrect command-line usage
#define EX_DATAERR  (65)    // lexer and parser errors
#define EX_NOINPUT  (66)    // invalid input file
#define EX_SOFTWARE (70)    // runtime errors
#define EX_IOERR    (74)    // I/O error


InterpretResult interpret(VM* vm, const char* src) {
    Chunk chunk;

    bool ok = compile(src, &chunk, vm);
    if (!ok) return INTERPRET_COMPILE_ERROR;

    return vm->interpret(&chunk);
}


void repl() {
    VM vm;

    while (true) {
        char *line = readline("> ");
        if (!line) break;

        if (*line) {
            add_history(line);
            interpret(&vm, line);
        }

        free(line);

        // printf("VM objects: %d\tstrings: %d / %d\n",
        //     vm.get_object_count(),
        //     vm.get_string_count(),
        //     vm.get_string_capacity());
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
    VM vm;

    char *file = read_file(path);
    int result = interpret(&vm, file);
    free(file);

    if (result == INTERPRET_COMPILE_ERROR) exit(EX_DATAERR);
    if (result == INTERPRET_RUNTIME_ERROR) exit(EX_SOFTWARE);
}

int main(int argc, const char* argv[]) {
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

