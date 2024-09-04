#include "arena.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#undef arena_alloc

Arena new_arena(ptrdiff_t size) {
    char *memory = malloc(size);
    if (!memory) {
        abort();
    }
    Arena arena = {0};
    arena.start = memory;
    arena.end = memory + size;
    return arena;
}

void delete_arena(Arena *arena) {
    free(arena->start);
}

void *arena_alloc(Arena *arena, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count) {
    ptrdiff_t padding = -(uintptr_t) arena->start & (align - 1);
    ptrdiff_t available = arena->end - arena->start - padding;
    if (available < 0 || count > available / size) {
        abort();
    }
    void *p = arena->start + padding;
    arena->start += padding + count * size;
    memset(p, 0, count * size);
    return p;
}
