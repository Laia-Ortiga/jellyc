#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline int64_t load_i64(uint32_t low, uint32_t high) {
    union {
        int64_t value;
        uint64_t bits;
    } result;
    result.bits = (uint64_t) low | ((uint64_t) high << 32);
    return result.value;
}

static inline double load_f64(uint32_t low, uint32_t high) {
    union {
        double value;
        uint64_t bits;
    } result;
    result.bits = (uint64_t) low | ((uint64_t) high << 32);
    return result.value;
}

static inline void store_i64(int64_t x, uint32_t *low, uint32_t *high) {
    *low = (uint32_t) x;
    *high = (uint32_t) ((uint64_t) x >> 32);
}

static inline void store_f64(double x, uint32_t *low, uint32_t *high) {
    union {
        double value;
        uint64_t bits;
    } u;
    u.value = x;
    *low = (uint32_t) u.bits;
    *high = (uint32_t) ((uint64_t) u.bits >> 32);
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
