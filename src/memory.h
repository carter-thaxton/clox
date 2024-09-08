#pragma once

#include "common.h"

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, ptr, old_capacity, new_capacity) \
    (type*) reallocate(ptr, sizeof(type) * (old_capacity), sizeof(type) * (new_capacity))

#define FREE_ARRAY(type, ptr, old_capacity) \
    (type*) reallocate(ptr, sizeof(type) * (old_capacity), 0)

void* reallocate(void* ptr, size_t old_size, size_t new_size);
