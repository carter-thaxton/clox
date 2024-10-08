#pragma once

#include "common.h"

struct VM;
struct ObjFunction;

ObjFunction* compile(const char* src, VM* vm);
void mark_compiler_roots();
