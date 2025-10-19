#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int64_t load_i64(void const *p) {
    int64_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static inline double load_f64(void const *p) {
    double value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static inline void store_i64(void *p, int64_t x) {
    memcpy(p, &x, sizeof(x));
}

static inline void store_f64(void *p, double x) {
    memcpy(p, &x, sizeof(x));
}

#define compiler_error(msg) \
    do { \
        fprintf(stderr, "Compiler error: %s\n", msg); \
        abort(); \
    } while (0)

#define compiler_error_fmt(fmt, ...) \
    do { \
        fprintf(stderr, "Compiler error: " fmt "\n", __VA_ARGS__); \
        abort(); \
    } while (0)
