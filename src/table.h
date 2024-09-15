#pragma once

#include "common.h"
#include "value.h"
#include "object.h"

struct Entry {
    ObjString* key;
    Value value;
};

struct Table {
    Table();
    ~Table();

    bool get(ObjString* key, Value* out_value); // return true if key found
    bool set(ObjString* key, Value value);      // return true if key found, does not insert otherwise
    bool insert(ObjString* key, Value value);   // return true if key not found, always inserts or overwrites
    int insert_all(Table* from);                // return count of new keys inserted, always inserts or overwrites all keys in from
    bool remove(ObjString* key);                // return true if key found and value removed

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
