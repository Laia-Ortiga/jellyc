#include "tir.h"

#include <stdlib.h>

TirId tir_push_generic(TirContext c, TirGeneric a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.inner, sizeof(a.inner));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.params.len;
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_GENERIC, data);
}

TirId tir_push_block(TirContext c, TirBlock a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    int32_t n = 1;
    n += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.stmts.len;
    memcpy(extra, a.stmts.ptr, a.stmts.len * sizeof(a.stmts.ptr[0]));
    extra += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_BLOCK, data);
}

TirId tir_push_array_type(TirContext c, TirArrayType a) {
    TirData data;
    memcpy(&data.a, &a.elem, sizeof(a.elem));
    memcpy(&data.b, &a.index, sizeof(a.index));
    return new_tir(c, TIR_ARRAY_TYPE, data);
}

TirId tir_push_array_length_type(TirContext c, TirArrayLengthType a) {
    TirData data;
    memcpy(&data.a, &a.length, sizeof(a.length));
    return new_tir(c, TIR_ARRAY_LENGTH_TYPE, data);
}

TirId tir_push_ptr_type(TirContext c, TirTag tag, TirPtrType a) {
    switch (tag) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.elem, sizeof(a.elem));
    return new_tir(c, tag, data);
}

TirId tir_push_slice_type(TirContext c, TirTag tag, TirSliceType a) {
    switch (tag) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.elem, sizeof(a.elem));
    memcpy(&data.b, &a.cached_ptr, sizeof(a.cached_ptr));
    return new_tir(c, tag, data);
}

TirId tir_push_function_type(TirContext c, TirFunctionType a) {
    TirData data;
    memcpy(&data.a, &a.ret, sizeof(a.ret));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.params.len;
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_FUNCTION_TYPE, data);
}

TirId tir_push_tagged_type(TirContext c, TirTaggedType a) {
    TirData data;
    memcpy(&data.a, &a.name, sizeof(a.name));
    memcpy(&data.b, &a.inner, sizeof(a.inner));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.args.len;
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_TAGGED_TYPE, data);
}

TirId tir_push_struct_type(TirContext c, TirStructType a) {
    TirData data;
    memcpy(&data.a, &a.scope, sizeof(a.scope));
    memcpy(&data.b, &a.name, sizeof(a.name));
    memcpy(&data.c, &a.alignment, sizeof(a.alignment));
    int32_t n = 4;
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.fields.len;
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    memcpy(extra, &a.size, sizeof(a.size));
    extra += sizeof(a.size) / sizeof(int32_t);
    memcpy(extra, &a.is_affine, sizeof(a.is_affine));
    extra += sizeof(a.is_affine) / sizeof(int32_t);
    return new_tir(c, TIR_STRUCT_TYPE, data);
}

TirId tir_push_enum_type(TirContext c, TirEnumType a) {
    TirData data;
    memcpy(&data.a, &a.scope, sizeof(a.scope));
    memcpy(&data.b, &a.name, sizeof(a.name));
    memcpy(&data.c, &a.repr, sizeof(a.repr));
    return new_tir(c, TIR_ENUM_TYPE, data);
}

TirId tir_push_affine_type(TirContext c, TirAffineType a) {
    TirData data;
    memcpy(&data.a, &a.elem, sizeof(a.elem));
    return new_tir(c, TIR_AFFINE_TYPE, data);
}

TirId tir_push_type_parameter(TirContext c, TirTypeParameter a) {
    TirData data;
    memcpy(&data.a, &a.index, sizeof(a.index));
    memcpy(&data.b, &a.name, sizeof(a.name));
    return new_tir(c, TIR_TYPE_PARAMETER, data);
}

TirId tir_push_function(TirContext c, TirFunction a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.name, sizeof(a.name));
    return new_tir(c, TIR_FUNCTION, data);
}

TirId tir_push_extern_function(TirContext c, TirExternFunction a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.name, sizeof(a.name));
    return new_tir(c, TIR_EXTERN_FUNCTION, data);
}

TirId tir_push_extern_var(TirContext c, TirExternVar a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.name, sizeof(a.name));
    return new_tir(c, TIR_EXTERN_VAR, data);
}

TirId tir_push_int(TirContext c, TirInt a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_tir(c, TIR_INT, data);
}

TirId tir_push_float(TirContext c, TirFloat a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_tir(c, TIR_FLOAT, data);
}

TirId tir_push_null(TirContext c, TirNull a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    return new_tir(c, TIR_NULL, data);
}

TirId tir_push_string(TirContext c, TirString a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_tir(c, TIR_STRING, data);
}

TirId tir_push_variable(TirContext c, TirTag tag, TirVariable a) {
    switch (tag) {
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.index, sizeof(a.index));
    return new_tir(c, tag, data);
}

TirId tir_push_let(TirContext c, TirLet a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.var, sizeof(a.var));
    memcpy(&data.d, &a.init, sizeof(a.init));
    return new_tir(c, TIR_LET, data);
}

TirId tir_push_unary(TirContext c, TirTag tag, TirUnary a) {
    switch (tag) {
        case TIR_PLUS:
        case TIR_MINUS:
        case TIR_NOT:
        case TIR_DEREF:
        case TIR_ADDRESS:
        case TIR_ADDRESS_OF_TEMPORARY:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.a, sizeof(a.a));
    return new_tir(c, tag, data);
}

TirId tir_push_binary(TirContext c, TirTag tag, TirBinary a) {
    switch (tag) {
        case TIR_ADD:
        case TIR_SUB:
        case TIR_MUL:
        case TIR_DIV:
        case TIR_MOD:
        case TIR_AND:
        case TIR_OR:
        case TIR_XOR:
        case TIR_SHL:
        case TIR_SHR:
        case TIR_EQ:
        case TIR_NE:
        case TIR_LT:
        case TIR_GT:
        case TIR_LE:
        case TIR_GE:
        case TIR_ASSIGN:
        case TIR_ASSIGN_ADD:
        case TIR_ASSIGN_SUB:
        case TIR_ASSIGN_MUL:
        case TIR_ASSIGN_DIV:
        case TIR_ASSIGN_MOD:
        case TIR_ASSIGN_AND:
        case TIR_ASSIGN_OR:
        case TIR_ASSIGN_XOR:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.a, sizeof(a.a));
    memcpy(&data.d, &a.b, sizeof(a.b));
    return new_tir(c, tag, data);
}

TirId tir_push_cast(TirContext c, TirTag tag, TirCast a) {
    switch (tag) {
        case TIR_ITOF:
        case TIR_ITRUNC:
        case TIR_SEXT:
        case TIR_ZEXT:
        case TIR_FTOI:
        case TIR_FTRUNC:
        case TIR_FEXT:
        case TIR_PTR_CAST:
        case TIR_NOP:
        case TIR_ARRAY_TO_SLICE:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.a, sizeof(a.a));
    return new_tir(c, tag, data);
}

TirId tir_push_call(TirContext c, TirCall a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.f, sizeof(a.f));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.args.len;
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_CALL, data);
}

TirId tir_push_index(TirContext c, TirIndex a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.a, sizeof(a.a));
    memcpy(&data.d, &a.index, sizeof(a.index));
    return new_tir(c, TIR_INDEX, data);
}

TirId tir_push_slice(TirContext c, TirSlice a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.a, sizeof(a.a));
    int32_t n = 2;
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, &a.low, sizeof(a.low));
    extra += sizeof(a.low) / sizeof(int32_t);
    memcpy(extra, &a.high, sizeof(a.high));
    extra += sizeof(a.high) / sizeof(int32_t);
    return new_tir(c, TIR_SLICE, data);
}

TirId tir_push_access(TirContext c, TirAccess a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.s, sizeof(a.s));
    memcpy(&data.d, &a.field, sizeof(a.field));
    return new_tir(c, TIR_ACCESS, data);
}

TirId tir_push_new_struct(TirContext c, TirNewStruct a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    int32_t n = 1;
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.fields.len;
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_NEW_STRUCT, data);
}

TirId tir_push_new_array(TirContext c, TirNewArray a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.args.len;
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_NEW_ARRAY, data);
}

TirId tir_push_if(TirContext c, TirIf a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.condition, sizeof(a.condition));
    int32_t n = 2;
    n += a.true_block.len * (sizeof(a.true_block.ptr[0]) / sizeof(int32_t));
    n += a.false_block.len * (sizeof(a.false_block.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.true_block.len;
    memcpy(extra, a.true_block.ptr, a.true_block.len * sizeof(a.true_block.ptr[0]));
    extra += a.true_block.len * (sizeof(a.true_block.ptr[0]) / sizeof(int32_t));
    *extra++ = a.false_block.len;
    memcpy(extra, a.false_block.ptr, a.false_block.len * sizeof(a.false_block.ptr[0]));
    extra += a.false_block.len * (sizeof(a.false_block.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_IF, data);
}

TirId tir_push_switch(TirContext c, TirSwitch a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.condition, sizeof(a.condition));
    int32_t n = 1;
    n += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    *extra++ = a.branches.len;
    memcpy(extra, a.branches.ptr, a.branches.len * sizeof(a.branches.ptr[0]));
    extra += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_SWITCH, data);
}

TirId tir_push_loop(TirContext c, TirLoop a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.init, sizeof(a.init));
    int32_t n = 3;
    n += a.block.len * (sizeof(a.block.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, &a.condition, sizeof(a.condition));
    extra += sizeof(a.condition) / sizeof(int32_t);
    memcpy(extra, &a.next, sizeof(a.next));
    extra += sizeof(a.next) / sizeof(int32_t);
    *extra++ = a.block.len;
    memcpy(extra, a.block.ptr, a.block.len * sizeof(a.block.ptr[0]));
    extra += a.block.len * (sizeof(a.block.ptr[0]) / sizeof(int32_t));
    return new_tir(c, TIR_LOOP, data);
}

TirId tir_push_break(TirContext c, TirBreak a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    return new_tir(c, TIR_BREAK, data);
}

TirId tir_push_continue(TirContext c, TirContinue a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    return new_tir(c, TIR_CONTINUE, data);
}

TirId tir_push_return(TirContext c, TirReturn a) {
    TirData data;
    memcpy(&data.a, &a.node, sizeof(a.node));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_tir(c, TIR_RETURN, data);
}

TirGeneric tir_get_generic(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_GENERIC:
            break;
        default:
            abort();
    }
    TirGeneric result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.inner, &get_term_data(c, a)->b, sizeof(result.inner));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.params.len = *extra++;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

TirBlock tir_get_block(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_BLOCK:
            break;
        default:
            abort();
    }
    TirBlock result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.stmts.len = *extra++;
    result.stmts.ptr = (void *) extra;
    extra += result.stmts.len * (sizeof(result.stmts.ptr[0]) / sizeof(int32_t));
    return result;
}

TirArrayType tir_get_array_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ARRAY_TYPE:
            break;
        default:
            abort();
    }
    TirArrayType result;
    memcpy(&result.elem, &get_term_data(c, a)->a, sizeof(result.elem));
    memcpy(&result.index, &get_term_data(c, a)->b, sizeof(result.index));
    return result;
}

TirArrayLengthType tir_get_array_length_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ARRAY_LENGTH_TYPE:
            break;
        default:
            abort();
    }
    TirArrayLengthType result;
    memcpy(&result.length, &get_term_data(c, a)->a, sizeof(result.length));
    return result;
}

TirPtrType tir_get_ptr_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
            break;
        default:
            abort();
    }
    TirPtrType result;
    memcpy(&result.elem, &get_term_data(c, a)->a, sizeof(result.elem));
    return result;
}

TirSliceType tir_get_slice_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
            break;
        default:
            abort();
    }
    TirSliceType result;
    memcpy(&result.elem, &get_term_data(c, a)->a, sizeof(result.elem));
    memcpy(&result.cached_ptr, &get_term_data(c, a)->b, sizeof(result.cached_ptr));
    return result;
}

TirFunctionType tir_get_function_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_FUNCTION_TYPE:
            break;
        default:
            abort();
    }
    TirFunctionType result;
    memcpy(&result.ret, &get_term_data(c, a)->a, sizeof(result.ret));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.params.len = *extra++;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

TirTaggedType tir_get_tagged_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_TAGGED_TYPE:
            break;
        default:
            abort();
    }
    TirTaggedType result;
    memcpy(&result.name, &get_term_data(c, a)->a, sizeof(result.name));
    memcpy(&result.inner, &get_term_data(c, a)->b, sizeof(result.inner));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.args.len = *extra++;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirStructType tir_get_struct_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_STRUCT_TYPE:
            break;
        default:
            abort();
    }
    TirStructType result;
    memcpy(&result.scope, &get_term_data(c, a)->a, sizeof(result.scope));
    memcpy(&result.name, &get_term_data(c, a)->b, sizeof(result.name));
    memcpy(&result.alignment, &get_term_data(c, a)->c, sizeof(result.alignment));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.fields.len = *extra++;
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    memcpy(&result.size, extra, sizeof(result.size));
    extra += sizeof(result.size) / sizeof(int32_t);
    memcpy(&result.is_affine, extra, sizeof(result.is_affine));
    extra += sizeof(result.is_affine) / sizeof(int32_t);
    return result;
}

TirEnumType tir_get_enum_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ENUM_TYPE:
            break;
        default:
            abort();
    }
    TirEnumType result;
    memcpy(&result.scope, &get_term_data(c, a)->a, sizeof(result.scope));
    memcpy(&result.name, &get_term_data(c, a)->b, sizeof(result.name));
    memcpy(&result.repr, &get_term_data(c, a)->c, sizeof(result.repr));
    return result;
}

TirAffineType tir_get_affine_type(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_AFFINE_TYPE:
            break;
        default:
            abort();
    }
    TirAffineType result;
    memcpy(&result.elem, &get_term_data(c, a)->a, sizeof(result.elem));
    return result;
}

TirTypeParameter tir_get_type_parameter(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_TYPE_PARAMETER:
            break;
        default:
            abort();
    }
    TirTypeParameter result;
    memcpy(&result.index, &get_term_data(c, a)->a, sizeof(result.index));
    memcpy(&result.name, &get_term_data(c, a)->b, sizeof(result.name));
    return result;
}

TirFunction tir_get_function(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_FUNCTION:
            break;
        default:
            abort();
    }
    TirFunction result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.name, &get_term_data(c, a)->c, sizeof(result.name));
    return result;
}

TirExternFunction tir_get_extern_function(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_EXTERN_FUNCTION:
            break;
        default:
            abort();
    }
    TirExternFunction result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.name, &get_term_data(c, a)->c, sizeof(result.name));
    return result;
}

TirExternVar tir_get_extern_var(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_EXTERN_VAR:
            break;
        default:
            abort();
    }
    TirExternVar result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.name, &get_term_data(c, a)->c, sizeof(result.name));
    return result;
}

TirInt tir_get_int(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_INT:
            break;
        default:
            abort();
    }
    TirInt result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.value, &get_term_data(c, a)->c, sizeof(result.value));
    return result;
}

TirFloat tir_get_float(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_FLOAT:
            break;
        default:
            abort();
    }
    TirFloat result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.value, &get_term_data(c, a)->c, sizeof(result.value));
    return result;
}

TirNull tir_get_null(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_NULL:
            break;
        default:
            abort();
    }
    TirNull result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    return result;
}

TirString tir_get_string(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_STRING:
            break;
        default:
            abort();
    }
    TirString result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.value, &get_term_data(c, a)->c, sizeof(result.value));
    return result;
}

TirVariable tir_get_variable(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
            break;
        default:
            abort();
    }
    TirVariable result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.index, &get_term_data(c, a)->c, sizeof(result.index));
    return result;
}

TirLet tir_get_let(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_LET:
            break;
        default:
            abort();
    }
    TirLet result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.var, &get_term_data(c, a)->c, sizeof(result.var));
    memcpy(&result.init, &get_term_data(c, a)->d, sizeof(result.init));
    return result;
}

TirUnary tir_get_unary(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_PLUS:
        case TIR_MINUS:
        case TIR_NOT:
        case TIR_DEREF:
        case TIR_ADDRESS:
        case TIR_ADDRESS_OF_TEMPORARY:
            break;
        default:
            abort();
    }
    TirUnary result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.a, &get_term_data(c, a)->c, sizeof(result.a));
    return result;
}

TirBinary tir_get_binary(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ADD:
        case TIR_SUB:
        case TIR_MUL:
        case TIR_DIV:
        case TIR_MOD:
        case TIR_AND:
        case TIR_OR:
        case TIR_XOR:
        case TIR_SHL:
        case TIR_SHR:
        case TIR_EQ:
        case TIR_NE:
        case TIR_LT:
        case TIR_GT:
        case TIR_LE:
        case TIR_GE:
        case TIR_ASSIGN:
        case TIR_ASSIGN_ADD:
        case TIR_ASSIGN_SUB:
        case TIR_ASSIGN_MUL:
        case TIR_ASSIGN_DIV:
        case TIR_ASSIGN_MOD:
        case TIR_ASSIGN_AND:
        case TIR_ASSIGN_OR:
        case TIR_ASSIGN_XOR:
            break;
        default:
            abort();
    }
    TirBinary result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.a, &get_term_data(c, a)->c, sizeof(result.a));
    memcpy(&result.b, &get_term_data(c, a)->d, sizeof(result.b));
    return result;
}

TirCast tir_get_cast(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ITOF:
        case TIR_ITRUNC:
        case TIR_SEXT:
        case TIR_ZEXT:
        case TIR_FTOI:
        case TIR_FTRUNC:
        case TIR_FEXT:
        case TIR_PTR_CAST:
        case TIR_NOP:
        case TIR_ARRAY_TO_SLICE:
            break;
        default:
            abort();
    }
    TirCast result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.a, &get_term_data(c, a)->c, sizeof(result.a));
    return result;
}

TirCall tir_get_call(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_CALL:
            break;
        default:
            abort();
    }
    TirCall result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.f, &get_term_data(c, a)->c, sizeof(result.f));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.args.len = *extra++;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirIndex tir_get_index(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_INDEX:
            break;
        default:
            abort();
    }
    TirIndex result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.a, &get_term_data(c, a)->c, sizeof(result.a));
    memcpy(&result.index, &get_term_data(c, a)->d, sizeof(result.index));
    return result;
}

TirSlice tir_get_slice(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_SLICE:
            break;
        default:
            abort();
    }
    TirSlice result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.a, &get_term_data(c, a)->c, sizeof(result.a));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    memcpy(&result.low, extra, sizeof(result.low));
    extra += sizeof(result.low) / sizeof(int32_t);
    memcpy(&result.high, extra, sizeof(result.high));
    extra += sizeof(result.high) / sizeof(int32_t);
    return result;
}

TirAccess tir_get_access(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_ACCESS:
            break;
        default:
            abort();
    }
    TirAccess result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.s, &get_term_data(c, a)->c, sizeof(result.s));
    memcpy(&result.field, &get_term_data(c, a)->d, sizeof(result.field));
    return result;
}

TirNewStruct tir_get_new_struct(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_NEW_STRUCT:
            break;
        default:
            abort();
    }
    TirNewStruct result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.fields.len = *extra++;
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

TirNewArray tir_get_new_array(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_NEW_ARRAY:
            break;
        default:
            abort();
    }
    TirNewArray result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.args.len = *extra++;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirIf tir_get_if(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_IF:
            break;
        default:
            abort();
    }
    TirIf result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.condition, &get_term_data(c, a)->c, sizeof(result.condition));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.true_block.len = *extra++;
    result.true_block.ptr = (void *) extra;
    extra += result.true_block.len * (sizeof(result.true_block.ptr[0]) / sizeof(int32_t));
    result.false_block.len = *extra++;
    result.false_block.ptr = (void *) extra;
    extra += result.false_block.len * (sizeof(result.false_block.ptr[0]) / sizeof(int32_t));
    return result;
}

TirSwitch tir_get_switch(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_SWITCH:
            break;
        default:
            abort();
    }
    TirSwitch result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.condition, &get_term_data(c, a)->c, sizeof(result.condition));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    result.branches.len = *extra++;
    result.branches.ptr = (void *) extra;
    extra += result.branches.len * (sizeof(result.branches.ptr[0]) / sizeof(int32_t));
    return result;
}

TirLoop tir_get_loop(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_LOOP:
            break;
        default:
            abort();
    }
    TirLoop result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.init, &get_term_data(c, a)->c, sizeof(result.init));
    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->d;
    memcpy(&result.condition, extra, sizeof(result.condition));
    extra += sizeof(result.condition) / sizeof(int32_t);
    memcpy(&result.next, extra, sizeof(result.next));
    extra += sizeof(result.next) / sizeof(int32_t);
    result.block.len = *extra++;
    result.block.ptr = (void *) extra;
    extra += result.block.len * (sizeof(result.block.ptr[0]) / sizeof(int32_t));
    return result;
}

TirBreak tir_get_break(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_BREAK:
            break;
        default:
            abort();
    }
    TirBreak result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    return result;
}

TirContinue tir_get_continue(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_CONTINUE:
            break;
        default:
            abort();
    }
    TirContinue result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    return result;
}

TirReturn tir_get_return(TirContext c, TirId a) {
    switch (get_tir_tag(c, a)) {
        case TIR_RETURN:
            break;
        default:
            abort();
    }
    TirReturn result;
    memcpy(&result.node, &get_term_data(c, a)->a, sizeof(result.node));
    memcpy(&result.type, &get_term_data(c, a)->b, sizeof(result.type));
    memcpy(&result.value, &get_term_data(c, a)->c, sizeof(result.value));
    return result;
}
