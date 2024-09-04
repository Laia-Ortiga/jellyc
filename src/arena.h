#pragma once

#include <stddef.h>

typedef struct {
    char *start;
    char *end;
} Arena;

Arena new_arena(ptrdiff_t size);
void delete_arena(Arena *arena);
void *arena_alloc(Arena *arena, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count);
#define arena_alloc(arena, type, count) ((type *) arena_alloc((arena), sizeof(type), _Alignof(type), (count)))
