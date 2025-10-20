#pragma once

#include "tir.h"

TirId remove_any_pointer(TirContext c, TirId a);
TirId remove_pointer(TirContext c, TirId a);
TirId remove_slice(TirContext c, TirId a);
TirId replace_slice_with_pointer(TirContext c, TirId a);
TirId replace_pointer_with_slice(TirContext c, TirId a);
TirId remove_c_pointer_like(TirContext c, TirId a);
TirId remove_array_like(TirContext c, TirId a);
TirId remove_tags(TirContext c, TirId a);

bool is_recursive_error_type(TirContext c, TirId a);
bool is_aggregate_type(TirContext c, TirId a);
bool type_is_affine(TirContext c, TirId a);
bool type_is_unknown_size(TirContext c, TirId a);
bool is_equality_type(TirContext c, TirId a);
bool is_relative_type(TirContext c, TirId a);

bool int_fits_in_type(int64_t i, TirId type, Target target);
int32_t sizeof_pointer(Target target);
int32_t alignof_type(TirContext c, TirId type, Target target);
int64_t sizeof_type(TirContext c, TirId type, Target target);

static inline bool type_is_fixed_int(TirId type) {
    return type.private_field_id >= RESERVED_i8 && type.private_field_id <= RESERVED_i64;
}

static inline bool type_is_int(TirId type) {
    return type_is_fixed_int(type) || type.private_field_id == RESERVED_isize;
}

static inline bool type_is_float(TirId type) {
    return type.private_field_id >= RESERVED_f32 && type.private_field_id <= RESERVED_f64;
}

static inline bool type_is_arithmetic(TirId type) {
    return type_is_int(type) || type_is_float(type);
}
