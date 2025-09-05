#include "hash.h"

#include "adt.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int32_t hash(String s) {
    int32_t hash = 5381;
    for (ptrdiff_t i = 0; i < s.len; i++) {
        hash = hash * 33 + s.ptr[i];
    }
    return hash;
}

static int32_t *get_key_lengths(HashTable const *table) {
    return table->data;
}

static char const **get_keys(HashTable const *table) {
    ptrdiff_t offset = table->capacity * sizeof(int32_t);
    offset += ~offset & (_Alignof(char const *) - 1);
    return (char const **) ((char*) table->data + offset);
}

static int32_t *get_values(HashTable const *table) {
    ptrdiff_t offset1 = table->capacity * sizeof(int32_t);
    offset1 += ~offset1 & (_Alignof(char const *) - 1);
    ptrdiff_t offset2 = offset1 + table->capacity * sizeof(char const *);
    offset2 += ~offset2 & (_Alignof(int32_t) - 1);
    return (int32_t *) ((char*) table->data + offset2);
}

static int32_t find_entry(HashTable const *table, String key) {
    int32_t index = hash(key) & (table->capacity - 1);
    int32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    for (;;) {
        if (!key_lengths[index]) {
            return -1;
        }
        if (equals((String) {key_lengths[index], keys[index]}, key)) {
            return index;
        }
        index = (index + 1) & (table->capacity - 1);
    }
    return -1;
}

static void htable_init_capacity(HashTable *table, int32_t capacity) {
    table->capacity = capacity;
    table->count = 0;
    ptrdiff_t offset1 = capacity * sizeof(int32_t);
    offset1 += ~offset1 & (_Alignof(char const *) - 1);
    ptrdiff_t offset2 = offset1 + capacity * sizeof(char const *);
    offset2 += ~offset2 & (_Alignof(int32_t) - 1);
    ptrdiff_t size = offset2 + capacity * sizeof(int32_t);
    table->data = malloc(size);
    if (!table->data) {
        abort();
    }
    memset(get_key_lengths(table), 0, capacity * sizeof(char const *));
}

HashTable htable_init(void) {
    HashTable table;
    htable_init_capacity(&table, 64);
    return table;
}

void htable_free(HashTable *table) {
    free(table->data);
}

void htable_insert_entry(HashTable *table, String key, int32_t value) {
    int32_t index = hash(key) & (table->capacity - 1);
    int32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    while (key_lengths[index]) {
        index = (index + 1) & (table->capacity - 1);
    }
    keys[index] = key.ptr;
    key_lengths[index] = key.len;
    get_values(table)[index] = value;
}

static void htable_resize(HashTable *table) {
    HashTable new_table;
    htable_init_capacity(&new_table, table->capacity * 2);
    new_table.count = table->count;
    int32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    int32_t *values = get_values(table);
    for (int32_t i = 0; i < table->capacity; i++) {
        if (key_lengths[i]) {
            htable_insert_entry(&new_table, (String) {key_lengths[i], keys[i]}, values[i]);
        }
    }
    htable_free(table);
    *table = new_table;
}

int64_t htable_try_insert(HashTable *table, String key, int32_t value) {
    if (table->count * 4 / table->capacity >= 3) {
        htable_resize(table);
    }

    int32_t index = hash(key) & (table->capacity - 1);
    int32_t *key_lengths = get_key_lengths(table);
    char const **keys = get_keys(table);
    int32_t *values = get_values(table);
    while (key_lengths[index]) {
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

int32_t *htable_lookup(HashTable const *table, String key) {
    int32_t entry = find_entry(table, key);
    if (entry == -1) {
        return NULL;
    }
    return get_values(table) + entry;
}
