#pragma once

#include "common.h"
#include "value.h"

struct Entry {
    ObjString* key;
    Value value;
};

struct Table {
    Table();
    ~Table();

    bool get(ObjString* key, Value* out_value);
    bool insert(ObjString* key, Value value);
    int insert_all(Table* from);
    bool remove(ObjString* key);

    ObjString* find_string(const char* str, int length, uint32_t hash);
    void adjust_capacity(int new_capacity);

    int get_capacity() { return capacity; }
    int get_count() { return count; }
    int get_count_with_tombstones() { return count_with_tombstones; }

private:
    Entry* entries;
    int capacity;
    int count;
    int count_with_tombstones;

    friend void print_table(Table* table);
};
