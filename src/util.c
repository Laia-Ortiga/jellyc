#include "util.h"

#include "adt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sum_vec_reserve(void *vec, int32_t count, size_t size) {
    SumVec(char) *internal = vec;
    int32_t old_cap = internal->cap;

    if (internal->len <= old_cap - count) {
        return;
    }

    if (internal->len > INT32_MAX - count) {
        abort();
    }

    int32_t new_capacity;
    if (old_cap > INT32_MAX - old_cap / 2) {
        new_capacity = INT32_MAX;
    } else if (old_cap / 2 >= count) {
        new_capacity = old_cap + old_cap / 2;
    } else {
        new_capacity = internal->len + count;
        int32_t min_cap = 16;

        if (new_capacity < min_cap) {
            new_capacity = min_cap;
        }
    }

    char *new_ptr = malloc(new_capacity * size + new_capacity);
    if (!new_ptr) {
        abort();
    }

    if (internal->datas) {
        memcpy(new_ptr, internal->datas, internal->len * size);
        memcpy(new_ptr + new_capacity * size, internal->datas + old_cap * size, internal->len);
        free(internal->datas);
    }

    internal->cap = new_capacity;
    internal->datas = new_ptr;
    internal->tags = (unsigned char *) (new_ptr + new_capacity * size);
}

ptrdiff_t push_str(StringBuffer *buffer, String s) {
    ptrdiff_t index = buffer->len;
    char *ptr = vec_grow(buffer, s.len);
    memcpy(ptr, s.ptr, s.len);
    return index;
}

#undef vec_grow

void *vec_grow(void *vec, int32_t count, size_t size) {
    Vec(char) *internal = vec;

    if (internal->len + count > internal->cap) {
        internal->cap += internal->cap / 2;
        if (internal->len + count > internal->cap) {
            internal->cap = internal->len + count;
            int32_t min_cap = 16;
            if (internal->cap < min_cap) {
                internal->cap = min_cap;
            }
        }

        char *new_ptr = malloc(internal->cap * size);
        if (!new_ptr) {
            abort();
        }

        if (internal->ptr) {
            memcpy(new_ptr, internal->ptr, internal->len * size);
            free(internal->ptr);
        }

        internal->ptr = new_ptr;
    }

    ptrdiff_t index = internal->len;
    internal->len += count;
    return internal->ptr + index * size;
}
