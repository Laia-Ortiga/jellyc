#include "type.h"

#include "mir.h"
#include "tir.h"

#include <stdlib.h>

TirId remove_any_pointer(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_pointer(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_slice(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId replace_slice_with_pointer(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_SLICE_TYPE: return new_ptr_type(c, tir_get_slice_type(c, type).elem);
        case TIR_MUT_SLICE_TYPE: return new_mut_ptr_type(c, tir_get_slice_type(c, type).elem);
        default: return error_term;
    }
}

TirId replace_pointer_with_slice(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_PTR_TYPE: return new_slice_type(c, tir_get_ptr_type(c, type).elem);
        case TIR_MUT_PTR_TYPE: return new_mut_slice_type(c, tir_get_ptr_type(c, type).elem);
        default: return error_term;
    }
}

TirId remove_c_pointer_like(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_ARRAY_TYPE: return tir_get_array_type(c, type).elem;
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_array_like(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_ARRAY_TYPE: return tir_get_array_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_tags(TirContext c, TirId type) {
    if (tir_get_tag(c, type) != TIR_TAGGED_TYPE) {
        return type;
    }

    return tir_get_tagged_type(c, type).inner;
}

int64_t type_get_domain_size(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ARRAY_LENGTH_TYPE: {
            return tir_get_array_length_type(c, a).length;
        }
        default: {
            return -1;
        }
    }
}

bool is_recursive_error_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_RESERVED:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_STRUCT_TYPE:
        case TIR_UNION_TYPE:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: {
            return false;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            return is_recursive_error_type(c, tir_get_ptr_type(c, a).elem);
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return is_recursive_error_type(c, tir_get_slice_type(c, a).elem);
        }
        case TIR_FUNCTION_TYPE: {
            TirFunctionType t = tir_get_function_type(c, a);
            for (int32_t i = 0; i < t.params.len; i++) {
                if (is_recursive_error_type(c, t.params.ptr[i])) {
                    return true;
                }
            }
            return is_recursive_error_type(c, t.ret);
        }
        case TIR_ARRAY_TYPE: {
            return is_recursive_error_type(c, tir_get_array_type(c, a).index)
                || is_recursive_error_type(c, tir_get_array_type(c, a).elem);
        }
        case TIR_TAGGED_TYPE: {
            TirTaggedType t = tir_get_tagged_type(c, a);
            for (int32_t i = 0; i < t.args.len; i++) {
                if (is_recursive_error_type(c, t.args.ptr[i])) {
                    return true;
                }
            }
            return false;
        }
        case TIR_AFFINE_TYPE: {
            return is_recursive_error_type(c, tir_get_affine_type(c, a).elem);
        }
        case TIR_ERROR:
        default: {
            return true;
        }
    }
}

bool is_aggregate_type(TirContext c, TirId type) {
    switch (tir_get_tag(c, type)) {
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case RESERVED_VOID:
                case RESERVED_i8:
                case RESERVED_i16:
                case RESERVED_i32:
                case RESERVED_i64:
                case RESERVED_byte:
                case RESERVED_isize:
                case RESERVED_f32:
                case RESERVED_f64:
                case RESERVED_bool: {
                    return false;
                }
                default: {
                    abort();
                }
            }
        }
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE: {
            return false;
        }
        case TIR_ARRAY_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_STRUCT_TYPE:
        case TIR_UNION_TYPE:
        case TIR_TYPE_PARAMETER: {
            return true;
        }
        case TIR_TAGGED_TYPE: {
            return is_aggregate_type(c, tir_get_tagged_type(c, type).inner);
        }
        case TIR_AFFINE_TYPE: {
            return is_aggregate_type(c, tir_get_affine_type(c, type).elem);
        }
        default: {
            abort();
        }
    }
}

bool type_is_affine(TirContext c, TirId type) {
    TirTag tag = tir_get_tag(c, type);
    switch (tag) {
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case RESERVED_VOID:
                case RESERVED_i8:
                case RESERVED_i16:
                case RESERVED_i32:
                case RESERVED_i64:
                case RESERVED_byte:
                case RESERVED_isize:
                case RESERVED_f32:
                case RESERVED_f64:
                case RESERVED_bool: {
                    return false;
                }
                default: {
                    abort();
                }
            }
        }
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: {
            return false;
        }
        case TIR_ARRAY_TYPE: {
            return type_is_affine(c, tir_get_array_type(c, type).elem);
        }
        case TIR_TAGGED_TYPE: {
            return type_is_affine(c, tir_get_tagged_type(c, type).inner);
        }
        case TIR_STRUCT_TYPE: {
            return tir_get_struct_type(c, type).is_affine;
        }
        case TIR_UNION_TYPE: {
            return tir_get_union_type(c, type).is_affine;
        }
        case TIR_AFFINE_TYPE: {
            return true;
        }
        default: {
            abort();
        }
    }
}

bool type_is_unknown_size(TirContext c, TirId type) {
    TirTag tag = tir_get_tag(c, type);
    switch (tag) {
        case TIR_ERROR: {
            return false;
        }
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case RESERVED_i8:
                case RESERVED_i16:
                case RESERVED_i32:
                case RESERVED_i64:
                case RESERVED_byte:
                case RESERVED_isize:
                case RESERVED_f32:
                case RESERVED_f64:
                case RESERVED_bool: {
                    return false;
                }
                default: {
                    abort();
                }
            }
        }
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_STRUCT_TYPE:
        case TIR_UNION_TYPE:
        case TIR_ENUM_TYPE: {
            return false;
        }
        case TIR_ARRAY_TYPE: {
            return type_is_unknown_size(c, tir_get_array_type(c, type).elem);
        }
        case TIR_TAGGED_TYPE: {
            return type_is_unknown_size(c, tir_get_tagged_type(c, type).inner);
        }
        case TIR_AFFINE_TYPE: {
            return type_is_unknown_size(c, tir_get_affine_type(c, type).elem);
        }
        case TIR_TYPE_PARAMETER: {
            return true;
        }
        default: {
            abort();
        }
    }
}

bool is_equality_type(TirContext c, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (tir_is_reserved(a, RESERVED_bool)) {
        return true;
    }
    if (tir_is_reserved(a, RESERVED_byte)) {
        return true;
    }
    switch (tir_get_tag(c, a)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE: {
            return true;
        }
        default: {
            return false;
        }
    }
}

bool is_relative_type(TirContext c, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    switch (tir_get_tag(c, a)) {
        case TIR_ENUM_TYPE: {
            return true;
        }
        default: {
            return false;
        }
    }
}

static bool int_fits_in_bytes(int64_t i, int bytes) {
    switch (bytes) {
        case 1: return i >= INT8_MIN && i <= INT8_MAX;
        case 2: return i >= INT16_MIN && i <= INT16_MAX;
        case 4: return i >= INT32_MIN && i <= INT32_MAX;
        case 8: return true;
        default: abort();
    }
}

static int64_t sizeof_primitive(TirId type, Target target) {
    switch (tir_as_reserved(type)) {
        case RESERVED_VOID: return -1;

        case RESERVED_i8:
        case RESERVED_bool:
        case RESERVED_byte: return 1;

        case RESERVED_i16: return 2;

        case RESERVED_i32:
        case RESERVED_f32: return 4;

        case RESERVED_i64:
        case RESERVED_f64: return 8;

        case RESERVED_isize: return sizeof_pointer(target);
        default: break;
    }
    abort();
}

bool int_fits_in_type(int64_t i, TirId type, Target target) {
    switch (tir_as_reserved(type)) {
        case RESERVED_i8:
        case RESERVED_i16:
        case RESERVED_i32:
        case RESERVED_i64:
        case RESERVED_isize: return int_fits_in_bytes(i, sizeof_primitive(type, target));

        default: return false;
    }
}

int32_t sizeof_pointer(Target target) {
    switch (target) {
        case TARGET_ISIZE_64: return 8;
        case TARGET_ISIZE_32: return 4;
    }
    abort();
}

int32_t alignof_type(Mir *mir, MirTypeId type, Target target) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8:
        case MIR_TYPE_BOOL:
        case MIR_TYPE_VOID: return 1;

        case MIR_TYPE_I16: return 2;

        case MIR_TYPE_I32:
        case MIR_TYPE_F32: return 4;

        case MIR_TYPE_I64:
        case MIR_TYPE_F64: return 8;

        case MIR_TYPE_PTR:
        case MIR_TYPE_SLICE: return sizeof_pointer(target);

        case MIR_TYPE_START: break;
    }

    MirTypeUnion u = get_mir_type(mir, type);
    switch (u.tag) {
        case MIR_TYPE_ARRAY: {
            return alignof_type(mir, u.array.elem, target);
        }
        case MIR_TYPE_FUNCTION: {
            return sizeof_pointer(target);
        }
        case MIR_TYPE_STRUCT: {
            return u.struct_.alignment;
        }
    }

    abort();
}

int64_t sizeof_type(Mir *mir, MirTypeId type, Target target) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8:
        case MIR_TYPE_BOOL: return 1;

        case MIR_TYPE_I16: return 2;

        case MIR_TYPE_I32:
        case MIR_TYPE_F32: return 4;

        case MIR_TYPE_I64:
        case MIR_TYPE_F64: return 8;

        case MIR_TYPE_VOID: return 0;

        case MIR_TYPE_PTR: return sizeof_pointer(target);
        case MIR_TYPE_SLICE: return 2 * sizeof_pointer(target);

        case MIR_TYPE_START: break;
    }

    MirTypeUnion u = get_mir_type(mir, type);
    switch (u.tag) {
        case MIR_TYPE_ARRAY: {
            int64_t elem_size = sizeof_type(mir, u.array.elem, target);
            int64_t size;
            if (__builtin_mul_overflow(u.array.length, elem_size, &size)) {
                return -1;
            }
            return size;
        }
        case MIR_TYPE_FUNCTION: {
            return sizeof_pointer(target);
        }
        case MIR_TYPE_STRUCT: {
            return u.struct_.size;
        }
    }

    abort();
}
