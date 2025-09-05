#pragma once

#include "adt.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t capacity;
    int32_t count;
    void *data;
} HashTable;

HashTable htable_init(void);
void htable_free(HashTable *table);
int64_t htable_try_insert(HashTable *table, String key, int32_t value);
int32_t *htable_lookup(HashTable const *table, String key);
