#pragma once

#include "chunk.h"

void Chunk_disassemble(Chunk* chunk, const char* name);
int Chunk_disassemble_instruction(Chunk* chunk, int offset);
void Value_print(Value value);
