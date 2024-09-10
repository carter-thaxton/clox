#include "table.h"
#include "memory.h"
#include <string.h>

#define TABLE_MAX_LOAD 0.75

Table::Table() {
    this->entries = NULL;
    this->capacity = 0;
    this->count = 0;
    this->count_with_tombstones = 0;
}

Table::~Table() {
    FREE_ARRAY(Entry, this->entries, this->capacity);

    this->entries = NULL;
    this->capacity = 0;
    this->count = 0;
    this->count_with_tombstones = 0;
}

static Entry* find_entry_helper(Entry* entries, int capacity, ObjString* key) {
    // TODO: use binary masking instead of modulo
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    while (true) {
        Entry* entry = &entries[index];
        if (entry->key == key) {
            // found
            return entry;
        } else if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // empty entry
                // return prior tombstone's entry if we already found one, otherwise use this location
                return tombstone ? tombstone : entry;
            } else if (tombstone == NULL) {
                // keep track of location of first tombstone found, and keep going
                tombstone = entry;
            }
        }
        index = (index + 1) % capacity;
    }
}

bool Table::get(ObjString* key, Value* out_value) {
    if (this->count == 0) return false;

    Entry* entry = find_entry_helper(this->entries, this->capacity, key);
    if (entry->key == NULL) return false;

    if (out_value) *out_value = entry->value;
    return true;
}

bool Table::set(ObjString* key, Value value) {
    if (this->count == 0) return false;

    Entry* entry = find_entry_helper(this->entries, this->capacity, key);
    if (entry->key == NULL) return false;

    entry->value = value;
    return true;
}

bool Table::insert(ObjString* key, Value value) {
    // grow table when it reaches max load
    if (count_with_tombstones + 1 > capacity * TABLE_MAX_LOAD) {
        int new_capacity = GROW_CAPACITY(this->capacity);
        adjust_capacity(new_capacity);
    }

    Entry* entry = find_entry_helper(this->entries, this->capacity, key);
    bool new_key = entry->key == NULL;
    entry->key = key;
    entry->value = value;

    if (new_key) {
        count++;
        if (IS_NIL(value)) count_with_tombstones++;
    }

    return new_key;
}

int Table::insert_all(Table* from) {
    // TODO: adjust_capacity once up front?
    int result = 0;
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key == NULL) continue;
        bool new_key = insert(entry->key, entry->value);
        if (new_key) result++;
    }
    return result;
}

bool Table::remove(ObjString* key) {
    if (this->count == 0) return false;

    Entry* entry = find_entry_helper(this->entries, this->capacity, key);
    if (entry->key == NULL) return false;

    // replace with tombstone
    // note: don't decrease count_with_tombstones on delete
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    count--;

    return true;
}

void Table::adjust_capacity(int new_capacity) {
    // allocate a new entries array
    Entry* new_entries = ALLOC_ARRAY(Entry, new_capacity);
    for (int i = 0; i < new_capacity; i++) {
        new_entries[i].key = NULL;
        new_entries[i].value = NIL_VAL;
    }

    // insert all old values into the new entries
    // calculate new count, without any tombstones
    int old_capacity = this->capacity;
    int new_count = 0;
    for (int i = 0; i < old_capacity; i++) {
        Entry* entry = &this->entries[i];
        if (entry->key == NULL) continue;  // also skip tombstones

        Entry* dest = find_entry_helper(new_entries, new_capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        new_count++;
    }

    // cleanup and use only the new entries
    FREE_ARRAY(Entry, this->entries, old_capacity);
    this->entries = new_entries;
    this->capacity = new_capacity;
    this->count = new_count;
    this->count_with_tombstones = new_count;
}

ObjString* Table::find_string(const char* str, int length, uint32_t hash) {
    if (count == 0) return NULL;

    uint32_t index = hash % capacity;
    while (true) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            // could stil be tombstone
            if (IS_NIL(entry->value)) {
                // empty non-tombstone entry
                return NULL;
            }
        } else if (entry->key->length == length && entry->key->hash == hash) {
            // length and hash match, now compare the actual string
            if (memcmp(entry->key->chars, str, length) == 0) {
                // found string key
                return entry->key;
            }
        }

        index = (index + 1) % capacity;
    }
}
