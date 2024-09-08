#include "debug.h"

#include <stdio.h>

static int print_simple_inst(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

void Chunk_disassemble(Chunk *chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->length; ) {
        offset = Chunk_disassemble_instruction(chunk, offset);
    }
}

int Chunk_disassemble_instruction(Chunk *chunk, int offset) {
    printf("%04d ", offset);

    uint8_t inst = chunk->code[offset];

    switch (inst) {
    case OP_RETURN:
        return print_simple_inst("OP_RETURN", offset);
    default:
        printf("Unknown opcode %d\n", inst);
        return offset + 1;
    }
}
