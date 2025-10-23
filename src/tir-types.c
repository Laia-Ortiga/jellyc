#include "tir.h"

#include <stdlib.h>

TirId tir_push_generic(TirContext c, TirGeneric a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.inner + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.params.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_GENERIC, data);
}

TirId tir_push_block(TirContext c, TirBlock a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.stmts.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, a.stmts.ptr, a.stmts.len * sizeof(a.stmts.ptr[0]));
    extra += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_BLOCK, data);
}

TirId tir_push_array_type(TirContext c, TirArrayType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.elem + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.index + 0, sizeof(int32_t));
    return tir_new(c, TIR_ARRAY_TYPE, data);
}

TirId tir_push_array_length_type(TirContext c, TirArrayLengthType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.length + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.length + 1, sizeof(int32_t));
    return tir_new(c, TIR_ARRAY_LENGTH_TYPE, data);
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
    memcpy(&data.a, (int32_t *) &a.elem + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
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
    memcpy(&data.a, (int32_t *) &a.elem + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.cached_ptr + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
}

TirId tir_push_function_type(TirContext c, TirFunctionType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.ret + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.params.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_FUNCTION_TYPE, data);
}

TirId tir_push_tagged_type(TirContext c, TirTaggedType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.name + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.inner + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.args.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_TAGGED_TYPE, data);
}

TirId tir_push_struct_type(TirContext c, TirStructType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.scope + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.name + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.has_public_fields + 0, sizeof(int32_t));
    int32_t n = 3;
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.file + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.fields.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.is_affine + 0, sizeof(int32_t));
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_STRUCT_TYPE, data);
}

TirId tir_push_union_type(TirContext c, TirUnionType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.scope + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.name + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.has_public_fields + 0, sizeof(int32_t));
    int32_t n = 3;
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.file + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.fields.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.is_affine + 0, sizeof(int32_t));
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_UNION_TYPE, data);
}

TirId tir_push_enum_type(TirContext c, TirEnumType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.scope + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.name + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.repr + 0, sizeof(int32_t));
    return tir_new(c, TIR_ENUM_TYPE, data);
}

TirId tir_push_affine_type(TirContext c, TirAffineType a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.elem + 0, sizeof(int32_t));
    return tir_new(c, TIR_AFFINE_TYPE, data);
}

TirId tir_push_type_parameter(TirContext c, TirTypeParameter a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.index + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.name + 0, sizeof(int32_t));
    return tir_new(c, TIR_TYPE_PARAMETER, data);
}

TirId tir_push_function(TirContext c, TirFunction a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.name + 0, sizeof(int32_t));
    return tir_new(c, TIR_FUNCTION, data);
}

TirId tir_push_extern_function(TirContext c, TirExternFunction a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.name + 0, sizeof(int32_t));
    return tir_new(c, TIR_EXTERN_FUNCTION, data);
}

TirId tir_push_extern_var(TirContext c, TirExternVar a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.name + 0, sizeof(int32_t));
    return tir_new(c, TIR_EXTERN_VAR, data);
}

TirId tir_push_int(TirContext c, TirInt a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.value + 1, sizeof(int32_t));
    return tir_new(c, TIR_INT, data);
}

TirId tir_push_float(TirContext c, TirFloat a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.value + 1, sizeof(int32_t));
    return tir_new(c, TIR_FLOAT, data);
}

TirId tir_push_null(TirContext c, TirNull a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    return tir_new(c, TIR_NULL, data);
}

TirId tir_push_string(TirContext c, TirString a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    return tir_new(c, TIR_STRING, data);
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
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.index + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
}

TirId tir_push_let(TirContext c, TirLet a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.var + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.init + 0, sizeof(int32_t));
    return tir_new(c, TIR_LET, data);
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
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.a + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
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
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.a + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.b + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
}

TirId tir_push_cast(TirContext c, TirTag tag, TirCast a) {
    switch (tag) {
        case TIR_CAST:
        case TIR_CHECKED_CAST:
        case TIR_UNSIGNED_CAST:
        case TIR_ARRAY_TO_SLICE:
            break;
        default:
            abort();
    }
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.a + 0, sizeof(int32_t));
    return tir_new(c, tag, data);
}

TirId tir_push_size_of(TirContext c, TirSizeOf a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.operand_type + 0, sizeof(int32_t));
    return tir_new(c, TIR_SIZE_OF, data);
}

TirId tir_push_align_of(TirContext c, TirAlignOf a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.operand_type + 0, sizeof(int32_t));
    return tir_new(c, TIR_ALIGN_OF, data);
}

TirId tir_push_call(TirContext c, TirCall a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.f + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.args.len + 0, sizeof(int32_t));
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_CALL, data);
}

TirId tir_push_index(TirContext c, TirIndex a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.a + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.index + 0, sizeof(int32_t));
    return tir_new(c, TIR_INDEX, data);
}

TirId tir_push_slice(TirContext c, TirSlice a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.a + 0, sizeof(int32_t));
    int32_t n = 2;
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.low + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.high + 0, sizeof(int32_t));
    return tir_new(c, TIR_SLICE, data);
}

TirId tir_push_access(TirContext c, TirAccess a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.s + 0, sizeof(int32_t));
    memcpy(&data.d, (int32_t *) &a.field + 0, sizeof(int32_t));
    return tir_new(c, TIR_ACCESS, data);
}

TirId tir_push_new_struct(TirContext c, TirNewStruct a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.field_indices.len + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.field_indices.len * (sizeof(a.field_indices.ptr[0]) / sizeof(int32_t));
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.fields.len + 0, sizeof(int32_t));
    memcpy(extra, a.field_indices.ptr, a.field_indices.len * sizeof(a.field_indices.ptr[0]));
    extra += a.field_indices.len * (sizeof(a.field_indices.ptr[0]) / sizeof(int32_t));
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_NEW_STRUCT, data);
}

TirId tir_push_new_array(TirContext c, TirNewArray a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.args.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_NEW_ARRAY, data);
}

TirId tir_push_if(TirContext c, TirIf a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.condition + 0, sizeof(int32_t));
    int32_t n = 2;
    n += a.true_block.len * (sizeof(a.true_block.ptr[0]) / sizeof(int32_t));
    n += a.false_block.len * (sizeof(a.false_block.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.true_block.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.false_block.len + 0, sizeof(int32_t));
    memcpy(extra, a.true_block.ptr, a.true_block.len * sizeof(a.true_block.ptr[0]));
    extra += a.true_block.len * (sizeof(a.true_block.ptr[0]) / sizeof(int32_t));
    memcpy(extra, a.false_block.ptr, a.false_block.len * sizeof(a.false_block.ptr[0]));
    extra += a.false_block.len * (sizeof(a.false_block.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_IF, data);
}

TirId tir_push_switch(TirContext c, TirSwitch a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.condition + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.branches.len + 0, sizeof(int32_t));
    memcpy(extra, a.branches.ptr, a.branches.len * sizeof(a.branches.ptr[0]));
    extra += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_SWITCH, data);
}

TirId tir_push_loop(TirContext c, TirLoop a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.init + 0, sizeof(int32_t));
    int32_t n = 3;
    n += a.block.len * (sizeof(a.block.ptr[0]) / sizeof(int32_t));
    data.d = tir_writer(c)->terms.extra.len;
    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);
    memcpy(extra++, (int32_t *) &a.condition + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.next + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.block.len + 0, sizeof(int32_t));
    memcpy(extra, a.block.ptr, a.block.len * sizeof(a.block.ptr[0]));
    extra += a.block.len * (sizeof(a.block.ptr[0]) / sizeof(int32_t));
    return tir_new(c, TIR_LOOP, data);
}

TirId tir_push_break(TirContext c, TirBreak a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    return tir_new(c, TIR_BREAK, data);
}

TirId tir_push_continue(TirContext c, TirContinue a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    return tir_new(c, TIR_CONTINUE, data);
}

TirId tir_push_return(TirContext c, TirReturn a) {
    TirData data;
    memcpy(&data.a, (int32_t *) &a.node + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    return tir_new(c, TIR_RETURN, data);
}

TirGeneric tir_get_generic(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_GENERIC:
            break;
        default:
            abort();
    }
    TirGeneric result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.inner + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.params.len + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

TirBlock tir_get_block(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_BLOCK:
            break;
        default:
            abort();
    }
    TirBlock result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.stmts.len + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    result.stmts.ptr = (void *) extra;
    extra += result.stmts.len * (sizeof(result.stmts.ptr[0]) / sizeof(int32_t));
    return result;
}

TirArrayType tir_get_array_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ARRAY_TYPE:
            break;
        default:
            abort();
    }
    TirArrayType result;
    memcpy((int32_t *) &result.elem + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.index + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirArrayLengthType tir_get_array_length_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ARRAY_LENGTH_TYPE:
            break;
        default:
            abort();
    }
    TirArrayLengthType result;
    memcpy((int32_t *) &result.length + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.length + 1, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirPtrType tir_get_ptr_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
            break;
        default:
            abort();
    }
    TirPtrType result;
    memcpy((int32_t *) &result.elem + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    return result;
}

TirSliceType tir_get_slice_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
            break;
        default:
            abort();
    }
    TirSliceType result;
    memcpy((int32_t *) &result.elem + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.cached_ptr + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirFunctionType tir_get_function_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_FUNCTION_TYPE:
            break;
        default:
            abort();
    }
    TirFunctionType result;
    memcpy((int32_t *) &result.ret + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.params.len + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

TirTaggedType tir_get_tagged_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_TAGGED_TYPE:
            break;
        default:
            abort();
    }
    TirTaggedType result;
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.inner + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.args.len + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirStructType tir_get_struct_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_STRUCT_TYPE:
            break;
        default:
            abort();
    }
    TirStructType result;
    memcpy((int32_t *) &result.scope + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.has_public_fields + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.file + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.fields.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.is_affine + 0, extra++, sizeof(int32_t));
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

TirUnionType tir_get_union_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_UNION_TYPE:
            break;
        default:
            abort();
    }
    TirUnionType result;
    memcpy((int32_t *) &result.scope + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.has_public_fields + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.file + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.fields.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.is_affine + 0, extra++, sizeof(int32_t));
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

TirEnumType tir_get_enum_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ENUM_TYPE:
            break;
        default:
            abort();
    }
    TirEnumType result;
    memcpy((int32_t *) &result.scope + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.repr + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirAffineType tir_get_affine_type(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_AFFINE_TYPE:
            break;
        default:
            abort();
    }
    TirAffineType result;
    memcpy((int32_t *) &result.elem + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    return result;
}

TirTypeParameter tir_get_type_parameter(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_TYPE_PARAMETER:
            break;
        default:
            abort();
    }
    TirTypeParameter result;
    memcpy((int32_t *) &result.index + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirFunction tir_get_function(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_FUNCTION:
            break;
        default:
            abort();
    }
    TirFunction result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirExternFunction tir_get_extern_function(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_EXTERN_FUNCTION:
            break;
        default:
            abort();
    }
    TirExternFunction result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirExternVar tir_get_extern_var(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_EXTERN_VAR:
            break;
        default:
            abort();
    }
    TirExternVar result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.name + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirInt tir_get_int(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_INT:
            break;
        default:
            abort();
    }
    TirInt result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 1, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirFloat tir_get_float(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_FLOAT:
            break;
        default:
            abort();
    }
    TirFloat result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 1, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirNull tir_get_null(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_NULL:
            break;
        default:
            abort();
    }
    TirNull result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirString tir_get_string(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_STRING:
            break;
        default:
            abort();
    }
    TirString result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirVariable tir_get_variable(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
            break;
        default:
            abort();
    }
    TirVariable result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.index + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirLet tir_get_let(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_LET:
            break;
        default:
            abort();
    }
    TirLet result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.var + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.init + 0, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirUnary tir_get_unary(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
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
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirBinary tir_get_binary(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
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
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.b + 0, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirCast tir_get_cast(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_CAST:
        case TIR_CHECKED_CAST:
        case TIR_UNSIGNED_CAST:
        case TIR_ARRAY_TO_SLICE:
            break;
        default:
            abort();
    }
    TirCast result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirSizeOf tir_get_size_of(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_SIZE_OF:
            break;
        default:
            abort();
    }
    TirSizeOf result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.operand_type + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirAlignOf tir_get_align_of(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ALIGN_OF:
            break;
        default:
            abort();
    }
    TirAlignOf result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.operand_type + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}

TirCall tir_get_call(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_CALL:
            break;
        default:
            abort();
    }
    TirCall result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.f + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.args.len + 0, extra++, sizeof(int32_t));
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirIndex tir_get_index(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_INDEX:
            break;
        default:
            abort();
    }
    TirIndex result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.index + 0, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirSlice tir_get_slice(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_SLICE:
            break;
        default:
            abort();
    }
    TirSlice result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.low + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.high + 0, extra++, sizeof(int32_t));
    return result;
}

TirAccess tir_get_access(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_ACCESS:
            break;
        default:
            abort();
    }
    TirAccess result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.s + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    memcpy((int32_t *) &result.field + 0, &tir_get_data(c, a)->d, sizeof(int32_t));
    return result;
}

TirNewStruct tir_get_new_struct(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_NEW_STRUCT:
            break;
        default:
            abort();
    }
    TirNewStruct result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.field_indices.len + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.fields.len + 0, extra++, sizeof(int32_t));
    result.field_indices.ptr = (void *) extra;
    extra += result.field_indices.len * (sizeof(result.field_indices.ptr[0]) / sizeof(int32_t));
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

TirNewArray tir_get_new_array(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_NEW_ARRAY:
            break;
        default:
            abort();
    }
    TirNewArray result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.args.len + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

TirIf tir_get_if(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_IF:
            break;
        default:
            abort();
    }
    TirIf result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.condition + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.true_block.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.false_block.len + 0, extra++, sizeof(int32_t));
    result.true_block.ptr = (void *) extra;
    extra += result.true_block.len * (sizeof(result.true_block.ptr[0]) / sizeof(int32_t));
    result.false_block.ptr = (void *) extra;
    extra += result.false_block.len * (sizeof(result.false_block.ptr[0]) / sizeof(int32_t));
    return result;
}

TirSwitch tir_get_switch(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_SWITCH:
            break;
        default:
            abort();
    }
    TirSwitch result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.condition + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.branches.len + 0, extra++, sizeof(int32_t));
    result.branches.ptr = (void *) extra;
    extra += result.branches.len * (sizeof(result.branches.ptr[0]) / sizeof(int32_t));
    return result;
}

TirLoop tir_get_loop(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_LOOP:
            break;
        default:
            abort();
    }
    TirLoop result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.init + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    int32_t *extra = tir_get_storage(c, a).tir->terms.extra.ptr + tir_get_data(c, a)->d;
    memcpy((int32_t *) &result.condition + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.next + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.block.len + 0, extra++, sizeof(int32_t));
    result.block.ptr = (void *) extra;
    extra += result.block.len * (sizeof(result.block.ptr[0]) / sizeof(int32_t));
    return result;
}

TirBreak tir_get_break(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_BREAK:
            break;
        default:
            abort();
    }
    TirBreak result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirContinue tir_get_continue(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_CONTINUE:
            break;
        default:
            abort();
    }
    TirContinue result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    return result;
}

TirReturn tir_get_return(TirContext c, TirId a) {
    switch (tir_get_tag(c, a)) {
        case TIR_RETURN:
            break;
        default:
            abort();
    }
    TirReturn result;
    memcpy((int32_t *) &result.node + 0, &tir_get_data(c, a)->a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &tir_get_data(c, a)->b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &tir_get_data(c, a)->c, sizeof(int32_t));
    return result;
}
