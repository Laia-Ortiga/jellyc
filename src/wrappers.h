#pragma once

#include "enums.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t id;
} DefId;

typedef struct {
    int32_t id;
} LocalId;

typedef struct {
    int32_t id;
} TirId;

typedef struct {
    int32_t private_field_id;
} MirId;

static TirId const null_tir = {0};

// Types

static TirId const type_void = {TYPE_VOID};

#define TYPE(type) static TirId const type_##type = {TYPE_##type};
#include "simple-types"

static inline bool type_is_fixed_int(TirId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_char;
}

static inline bool type_is_int(TirId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_isize;
}

static inline bool type_is_float(TirId type) {
    return type.id >= TYPE_f32 && type.id <= TYPE_f64;
}

static inline bool type_is_arithmetic(TirId type) {
    return type_is_int(type) || type_is_float(type);
}
