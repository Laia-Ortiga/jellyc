#pragma once

#include "enums.h"

#include <stdint.h>

typedef struct {
    int32_t private_field_id;
} AstId;

typedef struct {
    int32_t id;
} DefId;

typedef struct {
    int32_t id;
} LocalId;

typedef struct {
    int32_t id;
} TypeId;

typedef struct {
    int32_t id;
} ValueId;

typedef struct {
    int32_t id;
} TirId;

typedef struct {
    int32_t private_field_id;
} MirId;

static AstId const null_ast = {0};
static TypeId const null_type = {TYPE_INVALID};
static TirId const null_tir = {0};
static ValueId const null_value = {0};

static inline bool is_ast_null(AstId ast_id) {
    return !ast_id.private_field_id;
}

// Types

static TypeId const type_void = {TYPE_VOID};
static TypeId const type_size_tag = {TYPE_SIZE_TAG};
static TypeId const type_alignment_tag = {TYPE_ALIGNMENT_TAG};

#define TYPE(type) static TypeId const type_##type = {TYPE_##type};
#include "simple-types"

static inline bool type_is_fixed_int(TypeId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_char;
}

static inline bool type_is_int(TypeId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_isize;
}

static inline bool type_is_float(TypeId type) {
    return type.id >= TYPE_f32 && type.id <= TYPE_f64;
}

static inline bool type_is_arithmetic(TypeId type) {
    return type_is_int(type) || type_is_float(type);
}
