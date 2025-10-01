#include "type-match.h"

#include "tir.h"
#include "type.h"

static int match_types_single(TirContext c, TirId *results, TirId type, TypeMatcher *matcher) {
    if (!type.id) {
        return 0;
    }

    if (matcher->extra) {
        int32_t i = matcher->extra - 1;
        if (!results[i].id) {
            results[i] = type;
        } else if (results[i].id != type.id) {
            return 0;
        }
    }

    switch (matcher->match_type) {
        case TYPE_MATCH_T: {
            return 1;
        }
        case TYPE_MATCH_BYTE: {
            return type.id == TYPE_byte;
        }
        case TYPE_MATCH_ARRAY: {
            if (get_term_tag(c, type) == TIR_ARRAY_TYPE) {
                ArrayType array_type = get_array_type(c, type);
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
            if (get_term_tag(c, type) == TIR_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_SLICE: {
            if (get_term_tag(c, type) == TIR_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_POINTER: {
            if (get_term_tag(c, type) == TIR_MUT_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_SLICE: {
            if (get_term_tag(c, type) == TIR_MUT_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_TAGGED: {
            if (get_term_tag(c, type) == TIR_TAGGED_TYPE) {
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
    if (param.id == arg.id) {
        return 1;
    }

    TirTag tag = get_term_tag(c, param);
    if (tag == TIR_TYPE_PARAMETER) {
        int32_t index = get_type_parameter_index(c, param);
        if (!results[index].id) {
            results[index] = arg;
        } else if (results[index].id != arg.id) {
            return 0;
        }
        return 1;
    }
    if (tag != get_term_tag(c, arg)) {
        return 0;
    }
    switch (tag) {
        case TIR_RESERVED: {
            return 1;
        }
        case TIR_ARRAY_TYPE: {
            ArrayType param_array = get_array_type(c, param);
            ArrayType arg_array = get_array_type(c, arg);
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
            TirId param_elem = get_affine_elem_type(c, param);
            TirId arg_elem = get_affine_elem_type(c, arg);
            return match_type_parameters(c, results, param_elem, arg_elem);
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType param_f = get_function_type(c, param);
            FunctionType arg_f = get_function_type(c, arg);
            if (param_f.param_count != arg_f.param_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_f.param_count; i++) {
                if (!match_type_parameters(c, results, get_function_type_param(c, param, i), get_function_type_param(c, arg, i))) {
                    return 0;
                }
            }
            return match_type_parameters(c, results, param_f.ret, arg_f.ret);
        }
        case TIR_TAGGED_TYPE: {
            TaggedType param_t = get_tagged_type(c, param);
            TaggedType arg_t = get_tagged_type(c, arg);
            if (param_t.name != arg_t.name) {
                return 0;
            }
            if (param_t.arg_count != arg_t.arg_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_t.arg_count; i++) {
                if (!match_type_parameters(c, results, get_tagged_type_arg(c, param, i), get_tagged_type_arg(c, arg, i))) {
                    return 0;
                }
            }
            return 1;
        }
        case TIR_STRUCT_TYPE:
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
    switch (get_term_tag(info->c, generic)) {
        case TIR_RESERVED:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_ENUM_TYPE: {
            return generic;
        }
        case TIR_TYPE_PARAMETER: {
            int32_t index = get_type_parameter_index(info->c, generic);
            return info->args[index];
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(info->c, generic);
            return new_array_type(info->c, &(ArrayType) {
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
            TirId elem = get_affine_elem_type(info->c, generic);
            return new_affine_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType f = get_function_type(info->c, generic);
            TirId *params = arena_alloc(&info->scratch, TirId, f.param_count);
            for (int32_t i = 0; i < f.param_count; i++) {
                params[i] = replace_type_parameters(get_function_type_param(info->c, generic, i), info);
            }
            return new_function_type(info->c, &(FunctionType) {
                .param_count = f.param_count,
                .params = params,
                .ret = replace_type_parameters(f.ret, info),
            });
        }
        case TIR_TAGGED_TYPE: {
            TaggedType t = get_tagged_type(info->c, generic);
            TirId *tags = arena_alloc(&info->scratch, TirId, t.arg_count);
            for (int32_t i = 0; i < t.arg_count; i++) {
                tags[i] = replace_type_parameters(get_tagged_type_arg(info->c, generic, i), info);
            }
            TirId inner = replace_type_parameters(t.inner, info);
            return new_tagged_type(info->c, &(TaggedType) {
                .name = t.name,
                .inner = inner,
                .arg_count = t.arg_count,
                .args = tags,
            });
        }
        case TIR_STRUCT_TYPE: {
            StructType t = get_struct_type(info->c, generic);
            TirId *fields = arena_alloc(&info->scratch, TirId, t.field_count);
            for (int32_t i = 0; i < t.field_count; i++) {
                fields[i] = replace_type_parameters(get_struct_type_field(info->c, generic, i), info);
            }
            return new_struct_type(info->c, info->target, &(StructType) {
                .name = t.name,
                .scope = t.scope,
                .field_count = t.field_count,
                .fields = t.fields,
            });
        }
        default: {
            abort();
        }
    }
}
