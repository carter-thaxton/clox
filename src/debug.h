#pragma once

#include "chunk.h"
#include "value.h"

void print_chunk(Chunk* chunk, const char* name);
int  print_instruction(Chunk* chunk, int offset);
void print_value(Value value);
void print_object(Obj* object);
