#pragma once

#include "adt.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t capacity;
    size_t count;
    void *data;
} HashTable;

HashTable htable_init(void);
void htable_free(HashTable *table);
int64_t htable_try_insert(HashTable *table, String key, uint32_t value);
uint32_t *htable_lookup(HashTable const *table, String key);
