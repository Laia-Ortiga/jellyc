#include "hash.h"

#include "adt.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t hash(String s) {
    size_t hash = 5381;
    for (ptrdiff_t i = 0; i < s.len; i++) {
        hash = hash * 33 + s.ptr[i];
    }
    return hash;
}

static uint32_t *get_key_lengths(HashTable const *table) {
    return table->data;
}

static char const **get_keys(HashTable const *table) {
    size_t offset = table->capacity * sizeof(uint32_t);
    offset += ~offset & (_Alignof(char const *) - 1);
    return (char const **) ((char*) table->data + offset);
}

static uint32_t *get_values(HashTable const *table) {
    size_t offset1 = table->capacity * sizeof(uint32_t);
    offset1 += ~offset1 & (_Alignof(char const *) - 1);
    size_t offset2 = offset1 + table->capacity * sizeof(char const *);
    offset2 += ~offset2 & (_Alignof(uint32_t) - 1);
    return (uint32_t *) ((char*) table->data + offset2);
}

static size_t find_entry(HashTable const *table, String key) {
    size_t index = hash(key) & (table->capacity - 1);
    uint32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    for (;;) {
        if (!keys[index]) {
            return SIZE_MAX;
        }
        if (equals((String) {key_lengths[index], keys[index]}, key)) {
            return index;
        }
        index = (index + 1) & (table->capacity - 1);
    }
    return SIZE_MAX;
}

static void htable_init_capacity(HashTable *table, size_t capacity) {
    table->capacity = capacity;
    table->count = 0;
    size_t offset1 = capacity * sizeof(uint32_t);
    offset1 += ~offset1 & (_Alignof(char const *) - 1);
    size_t offset2 = offset1 + capacity * sizeof(char const *);
    offset2 += ~offset2 & (_Alignof(uint32_t) - 1);
    size_t size = offset2 + capacity * sizeof(uint32_t);
    table->data = malloc(size);
    if (!table->data) {
        abort();
    }
    memset(get_keys(table), 0, capacity * sizeof(char const *));
}

HashTable htable_init(void) {
    HashTable table;
    htable_init_capacity(&table, 64);
    return table;
}

void htable_free(HashTable *table) {
    free(table->data);
}

void htable_insert_entry(HashTable *table, String key, uint32_t value) {
    size_t index = hash(key) & (table->capacity - 1);
    char const **keys = get_keys(table);
    while (keys[index]) {
        index = (index + 1) & (table->capacity - 1);
    }
    keys[index] = key.ptr;
    get_key_lengths(table)[index] = key.len;
    get_values(table)[index] = value;
}

static void htable_resize(HashTable *table) {
    HashTable new_table;
    htable_init_capacity(&new_table, table->capacity * 2);
    new_table.count = table->count;
    uint32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    uint32_t *values = get_values(table);
    for (size_t i = 0; i < table->capacity; i++) {
        if (keys[i]) {
            htable_insert_entry(&new_table, (String) {key_lengths[i], keys[i]}, values[i]);
        }
    }
    htable_free(table);
    *table = new_table;
}

int64_t htable_try_insert(HashTable *table, String key, uint32_t value) {
    if (table->count * 4 / table->capacity >= 3) {
        htable_resize(table);
    }

    size_t index = hash(key) & (table->capacity - 1);
    uint32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    uint32_t *values = get_values(table);
    while (keys[index]) {
        if (equals((String) {key_lengths[index], keys[index]}, key)) {
            return values[index];
        }
        index = (index + 1) & (table->capacity - 1);
    }
    key_lengths[index] = key.len;
    keys[index] = key.ptr;
    values[index] = value;
    table->count++;
    return -1;
}

uint32_t *htable_lookup(HashTable const *table, String key) {
    size_t entry = find_entry(table, key);
    if (entry == SIZE_MAX) {
        return NULL;
    }
    return get_values(table) + entry;
}
