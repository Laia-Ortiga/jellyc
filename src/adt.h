#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define Str(S) {sizeof(S) - 1, S}
#define ArrayLength(A) ((ptrdiff_t) (sizeof(A) / sizeof(A[0])))
#define Vec(T) struct { int32_t cap; int32_t len; T *ptr; }
#define SumVec(T) struct { int32_t cap; int32_t len; unsigned char *tags; T *datas; }



typedef struct {
    ptrdiff_t len;
    char const *ptr;
} String;

static inline bool equals(String a, String b) {
    return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

static inline String substring(String s, ptrdiff_t start, ptrdiff_t end) {
    return (String) {.len = end - start, .ptr = s.ptr + start};
}



typedef Vec(char) StringBuffer;

void sum_vec_reserve(void *vec, int32_t count, size_t size);
void *vec_grow(void *vec, int32_t count, size_t size);
ptrdiff_t push_str(StringBuffer *buffer, String s);



#define vec_grow(vec, count) ((typeof((vec)->ptr)) vec_grow((vec), (count), sizeof(*(vec)->ptr)))

#define vec_push(vec, ...) (*((typeof((vec)->ptr)) vec_grow((vec), 1)) = (__VA_ARGS__))

#define sum_vec_push(vec, data, tag) \
    do { \
        sum_vec_reserve((vec), 1, sizeof((vec)->datas[0])); \
        (vec)->datas[(vec)->len] = (data); \
        (vec)->tags[(vec)->len] = (tag); \
        (vec)->len++; \
    } while (0)
