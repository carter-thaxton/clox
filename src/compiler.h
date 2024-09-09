#pragma once

#include "common.h"

struct Chunk;
struct VM;

bool compile(const char* src, Chunk* chunk, VM* vm);
