#include "type.h"

#include "tir.h"

#include <stdlib.h>

TirId remove_any_pointer(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_pointer(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_slice(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId replace_slice_with_pointer(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_SLICE_TYPE: return new_ptr_type(c, tir_get_slice_type(c, type).elem);
        case TIR_MUT_SLICE_TYPE: return new_mut_ptr_type(c, tir_get_slice_type(c, type).elem);
        default: return error_term;
    }
}

TirId replace_pointer_with_slice(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_PTR_TYPE: return new_slice_type(c, tir_get_ptr_type(c, type).elem);
        case TIR_MUT_PTR_TYPE: return new_mut_slice_type(c, tir_get_ptr_type(c, type).elem);
        default: return error_term;
    }
}

TirId remove_c_pointer_like(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_ARRAY_TYPE: return tir_get_array_type(c, type).elem;
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return tir_get_ptr_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_array_like(TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_ARRAY_TYPE: return tir_get_array_type(c, type).elem;
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return tir_get_slice_type(c, type).elem;

        default: return error_term;
    }
}

TirId remove_tags(TirContext c, TirId type) {
    if (get_tir_tag(c, type) != TIR_TAGGED_TYPE) {
        return type;
    }

    return tir_get_tagged_type(c, type).inner;
}

bool is_recursive_error_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_RESERVED:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_STRUCT_TYPE:
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
    switch (get_tir_tag(c, type)) {
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case TYPE_VOID:
                case TYPE_i8:
                case TYPE_i16:
                case TYPE_i32:
                case TYPE_i64:
                case TYPE_byte:
                case TYPE_isize:
                case TYPE_f32:
                case TYPE_f64:
                case TYPE_bool: {
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
    TirTag tag = get_tir_tag(c, type);
    switch (tag) {
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case TYPE_VOID:
                case TYPE_i8:
                case TYPE_i16:
                case TYPE_i32:
                case TYPE_i64:
                case TYPE_byte:
                case TYPE_isize:
                case TYPE_f32:
                case TYPE_f64:
                case TYPE_bool: {
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
        case TIR_AFFINE_TYPE: {
            return true;
        }
        default: {
            abort();
        }
    }
}

bool type_is_unknown_size(TirContext c, TirId type) {
    TirTag tag = get_tir_tag(c, type);
    switch (tag) {
        case TIR_ERROR: {
            return false;
        }
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case TYPE_i8:
                case TYPE_i16:
                case TYPE_i32:
                case TYPE_i64:
                case TYPE_byte:
                case TYPE_isize:
                case TYPE_f32:
                case TYPE_f64:
                case TYPE_bool: {
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
    if (tir_is_reserved(a, TYPE_bool)) {
        return true;
    }
    if (tir_is_reserved(a, TYPE_byte)) {
        return true;
    }
    switch (get_tir_tag(c, a)) {
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
    switch (get_tir_tag(c, a)) {
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
        case TYPE_VOID: return -1;

        case TYPE_i8:
        case TYPE_bool:
        case TYPE_byte: return 1;

        case TYPE_i16: return 2;

        case TYPE_i32:
        case TYPE_f32: return 4;

        case TYPE_i64:
        case TYPE_f64: return 8;

        case TYPE_isize: return sizeof_pointer(target);
        default: break;
    }
    abort();
}

bool int_fits_in_type(int64_t i, TirId type, Target target) {
    switch (tir_as_reserved(type)) {
        case TYPE_i8:
        case TYPE_i16:
        case TYPE_i32:
        case TYPE_i64:
        case TYPE_isize: return int_fits_in_bytes(i, sizeof_primitive(type, target));

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

int32_t alignof_type(TirContext c, TirId type, Target target) {
    switch (get_tir_tag(c, type)) {
        case TIR_RESERVED: return -1;
        case TIR_TYPE_PARAMETER: return -1;

        default: {
            switch (tir_as_reserved(type)) {
                case TYPE_VOID: return -1;

                case TYPE_i8:
                case TYPE_bool:
                case TYPE_byte: return 1;

                case TYPE_i16: return 2;

                case TYPE_i32:
                case TYPE_f32: return 4;

                case TYPE_i64:
                case TYPE_f64: return 8;

                case TYPE_isize: return sizeof_pointer(target);

                default: break;
            }
            abort();
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return sizeof_pointer(target);

        case TIR_ARRAY_TYPE: return alignof_type(c, tir_get_array_type(c, type).elem, target);
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return tir_get_struct_type(c, type).alignment;
        case TIR_ENUM_TYPE: return alignof_type(c, tir_get_enum_type(c, type).repr, target);
        case TIR_TAGGED_TYPE: return alignof_type(c, tir_get_tagged_type(c, type).inner, target);
        case TIR_AFFINE_TYPE: return alignof_type(c, tir_get_affine_type(c, type).elem, target);
    }
}

int64_t sizeof_type(TirContext c, TirId type, Target target) {
    switch (get_tir_tag(c, type)) {
        case TIR_RESERVED: return sizeof_primitive(type, target);

        case TIR_TYPE_PARAMETER: return -1;

        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE: return sizeof_pointer(target);

        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return 2 * sizeof_pointer(target);

        case TIR_ARRAY_TYPE: {
            TirArrayType array = tir_get_array_type(c, type);
            int64_t length = tir_get_array_length_type(c, array.index).length;
            return length * sizeof_type(c, array.elem, target);
        }
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return tir_get_struct_type(c, type).size;
        case TIR_ENUM_TYPE: return sizeof_type(c, tir_get_enum_type(c, type).repr, target);
        case TIR_TAGGED_TYPE: return sizeof_type(c, tir_get_tagged_type(c, type).inner, target);
        case TIR_AFFINE_TYPE: return sizeof_type(c, tir_get_affine_type(c, type).elem, target);
        default: {
            abort();
        }
    }
}
