#pragma once

#include "chunk.h"
#include "value.h"
#include "table.h"
#include <stdio.h>

void print_chunk(Chunk* chunk, const char* name);
int  print_instruction(Chunk* chunk, int offset);
void print_value(Value value);
void print_value_array(ValueArray* array);
void print_object(Obj* object);
void print_table(Table* table);
void print_strings(Table* table);
