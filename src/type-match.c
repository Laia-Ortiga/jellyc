#include "type-match.h"

#include "fwd.h"
#include "tir.h"
#include "type.h"

#include <stdlib.h>

static int match_types_single(TirContext c, TirId *results, TirId type, TypeMatcher *matcher) {
    if (tir_is_reserved(type, RESERVED_ERROR)) {
        return 0;
    }

    if (matcher->extra) {
        int32_t i = matcher->extra - 1;
        if (tir_is_reserved(results[i], RESERVED_ERROR)) {
            results[i] = type;
        } else if (!tir_eq(results[i], type)) {
            return 0;
        }
    }

    switch (matcher->match_type) {
        case TYPE_MATCH_T: {
            return 1;
        }
        case TYPE_MATCH_BYTE: {
            return tir_is_reserved(type, RESERVED_byte);
        }
        case TYPE_MATCH_ARRAY: {
            if (get_tir_tag(c, type) == TIR_ARRAY_TYPE) {
                TirArrayType array_type = tir_get_array_type(c, type);
                return match_types_single(c, results, array_type.index, &matcher->inner[0])
                    && match_types_single(c, results, array_type.elem, &matcher->inner[1]);
            }
            return 0;
        }
        case TYPE_MATCH_ANY_POINTER: {
            TirId inner = remove_pointer(c, type);
            return match_types_single(c, results, inner, matcher->inner);
        }
        case TYPE_MATCH_ANY_SLICE: {
            TirId inner = remove_slice(c, type);
            return match_types_single(c, results, inner, matcher->inner);
        }
        case TYPE_MATCH_POINTER: {
            if (get_tir_tag(c, type) == TIR_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_SLICE: {
            if (get_tir_tag(c, type) == TIR_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_POINTER: {
            if (get_tir_tag(c, type) == TIR_MUT_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_SLICE: {
            if (get_tir_tag(c, type) == TIR_MUT_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_TAGGED: {
            if (get_tir_tag(c, type) == TIR_TAGGED_TYPE) {
                TirId inner = remove_tags(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
    }
    return 0;
}

int match_types(TirContext c, TirId *results, int32_t count, TirId *types, TypeMatcher *matchers) {
    for (int32_t i = 0; i < count; i++) {
        if (!match_types_single(c, results, types[i], &matchers[i])) {
            return 0;
        }
    }
    return 1;
}

int match_type_parameters(TirContext c, TirId *results, TirId param, TirId arg) {
    if (tir_eq(param, arg)) {
        return 1;
    }

    TirTag tag = get_tir_tag(c, param);
    if (tag == TIR_TYPE_PARAMETER) {
        int32_t index = tir_get_type_parameter(c, param).index;
        if (tir_is_reserved(results[index], RESERVED_ERROR)) {
            results[index] = arg;
        } else if (!tir_eq(results[index], arg)) {
            return 0;
        }
        return 1;
    }
    if (tag != get_tir_tag(c, arg)) {
        return 0;
    }
    switch (tag) {
        case TIR_RESERVED: {
            return 1;
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType param_array = tir_get_array_type(c, param);
            TirArrayType arg_array = tir_get_array_type(c, arg);
            return match_type_parameters(c, results, param_array.index, arg_array.index)
                && match_type_parameters(c, results, param_array.elem, arg_array.elem);
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return 0;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            TirId param_elem = remove_pointer(c, param);
            TirId arg_elem = remove_pointer(c, arg);
            return match_type_parameters(c, results, param_elem, arg_elem);
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            TirId param_elem = remove_slice(c, param);
            TirId arg_elem = remove_slice(c, arg);
            return match_type_parameters(c, results, param_elem, arg_elem);
        }
        case TIR_AFFINE_TYPE: {
            TirId param_elem = tir_get_affine_type(c, param).elem;
            TirId arg_elem = tir_get_affine_type(c, arg).elem;
            return match_type_parameters(c, results, param_elem, arg_elem);
        }
        case TIR_FUNCTION_TYPE: {
            TirFunctionType param_f = tir_get_function_type(c, param);
            TirFunctionType arg_f = tir_get_function_type(c, arg);
            if (param_f.params.len != arg_f.params.len) {
                return 0;
            }
            for (int32_t i = 0; i < param_f.params.len; i++) {
                if (!match_type_parameters(c, results, get_function_type_param(c, param, i), get_function_type_param(c, arg, i))) {
                    return 0;
                }
            }
            return match_type_parameters(c, results, param_f.ret, arg_f.ret);
        }
        case TIR_TAGGED_TYPE: {
            TirTaggedType param_t = tir_get_tagged_type(c, param);
            TirTaggedType arg_t = tir_get_tagged_type(c, arg);
            if (param_t.name != arg_t.name) {
                return 0;
            }
            if (param_t.args.len != arg_t.args.len) {
                return 0;
            }
            for (int32_t i = 0; i < param_t.args.len; i++) {
                if (!match_type_parameters(c, results, get_tagged_type_arg(c, param, i), get_tagged_type_arg(c, arg, i))) {
                    return 0;
                }
            }
            return 1;
        }
        case TIR_STRUCT_TYPE:
        case TIR_UNION_TYPE:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: {
            break;
        }
        default: {
            abort();
        }
    }
    return 0;
}

TirId replace_type_parameters(TirId generic, ReplaceTypeInfo *info) {
    switch (get_tir_tag(info->c, generic)) {
        case TIR_RESERVED:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_ENUM_TYPE: {
            return generic;
        }
        case TIR_TYPE_PARAMETER: {
            int32_t index = tir_get_type_parameter(info->c, generic).index;
            return info->args[index];
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType array = tir_get_array_type(info->c, generic);
            return new_array_type(info->c, (TirArrayType) {
                .index = replace_type_parameters(array.index, info),
                .elem = replace_type_parameters(array.elem, info),
            });
        }
        case TIR_PTR_TYPE: {
            TirId elem = remove_pointer(info->c, generic);
            return new_ptr_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_MUT_PTR_TYPE: {
            TirId elem = remove_pointer(info->c, generic);
            return new_mut_ptr_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_SLICE_TYPE: {
            TirId elem = remove_slice(info->c, generic);
            return new_slice_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_MUT_SLICE_TYPE: {
            TirId elem = remove_slice(info->c, generic);
            return new_mut_slice_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_AFFINE_TYPE: {
            TirId elem = tir_get_affine_type(info->c, generic).elem;
            return new_affine_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_FUNCTION_TYPE: {
            TirFunctionType f = tir_get_function_type(info->c, generic);
            TirId *params = arena_alloc(&info->scratch, TirId, f.params.len);
            for (int32_t i = 0; i < f.params.len; i++) {
                params[i] = replace_type_parameters(get_function_type_param(info->c, generic, i), info);
            }
            return new_function_type(info->c, (TirFunctionType) {
                .params = {f.params.len, params},
                .ret = replace_type_parameters(f.ret, info),
            });
        }
        case TIR_TAGGED_TYPE: {
            TirTaggedType t = tir_get_tagged_type(info->c, generic);
            TirId *tags = arena_alloc(&info->scratch, TirId, t.args.len);
            for (int32_t i = 0; i < t.args.len; i++) {
                tags[i] = replace_type_parameters(get_tagged_type_arg(info->c, generic, i), info);
            }
            TirId inner = replace_type_parameters(t.inner, info);
            return new_tagged_type(info->c, (TirTaggedType) {
                .name = t.name,
                .inner = inner,
                .args = {t.args.len, tags},
            });
        }
        case TIR_STRUCT_TYPE: {
            TirStructType t = tir_get_struct_type(info->c, generic);
            TirId *fields = arena_alloc(&info->scratch, TirId, t.fields.len);
            for (int32_t i = 0; i < t.fields.len; i++) {
                fields[i] = replace_type_parameters(get_struct_type_field(info->c, generic, i), info);
            }
            return new_struct_type(info->c, (TirStructType) {
                .name = t.name,
                .scope = t.scope,
                .fields = {t.fields.len, fields},
            });
        }
        case TIR_UNION_TYPE: {
            TirUnionType t = tir_get_union_type(info->c, generic);
            TirId *fields = arena_alloc(&info->scratch, TirId, t.fields.len);
            for (int32_t i = 0; i < t.fields.len; i++) {
                fields[i] = replace_type_parameters(get_struct_type_field(info->c, generic, i), info);
            }
            return new_union_type(info->c, (TirUnionType) {
                .name = t.name,
                .scope = t.scope,
                .fields = {t.fields.len, fields},
            });
        }
        default: {
            abort();
        }
    }
}
