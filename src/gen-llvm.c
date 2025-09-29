#include "gen.h"

#include "adt.h"
#include "mir.h"
#include "tir.h"
#include "fwd.h"

#include <assert.h>
#include <stdlib.h>

typedef enum : char {
    OPERAND_INT,
    OPERAND_TIR,
    OPERAND_TMP,
} OperandTag;

typedef struct {
    bool is_lvalue;
    OperandTag tag;
    TirId type;
    union {
        int64_t i;
        TirId value;
        int32_t index;
    };
} Operand;

typedef struct {
    Mir *mir;
    Vec(Operand) stack;
    int32_t data_top;
    int32_t tmp_count;
    int32_t alloc_count;
    Vec(char const *) strings;
    int32_t blocks;
    bool is_main;
    Target target;
    TirContext tir;
    FILE *stream;
} GenContext;

static void gen_type(GenContext *ctx, TirId type) {
    switch (get_term_tag(ctx->tir, type)) {
        case TIR_PRIMITIVE_TYPE: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_VOID: fprintf(ctx->stream, "void"); return;

                case TYPE_i8:
                case TYPE_byte: fprintf(ctx->stream, "i8"); return;

                case TYPE_i16: fprintf(ctx->stream, "i16"); return;
                case TYPE_i32: fprintf(ctx->stream, "i32"); return;
                case TYPE_i64: fprintf(ctx->stream, "i64"); return;

                case TYPE_isize: fprintf(ctx->stream, "i%d", sizeof_pointer(ctx->target) * 8); return;

                case TYPE_f32: fprintf(ctx->stream, "float"); return;
                case TYPE_f64: fprintf(ctx->stream, "double"); return;
                case TYPE_bool: fprintf(ctx->stream, "i1"); return;

                default: break;
            }
        }
        case TIR_TYPE_PARAMETER: {
            break;
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(ctx->tir, type);
            int64_t length = get_array_length_type(ctx->tir, array.index);
            fprintf(ctx->stream, "[%ld x ", length);
            gen_type(ctx, array.elem);
            fprintf(ctx->stream, "]");
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            fprintf(ctx->stream, "i%d", sizeof_pointer(ctx->target) * 8);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE: {
            fprintf(ctx->stream, "ptr");
            return;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            fprintf(ctx->stream, "%%slice");
            return;
        }
        case TIR_STRUCT_TYPE: {
            fprintf(ctx->stream, "%%_%s", tir_get_str(ctx->tir, get_struct_type(ctx->tir, type).name));
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type(ctx, get_affine_elem_type(ctx->tir, type));
            return;
        }
        default: {
            abort();
        }
    }
}

static void gen_params(GenContext *ctx, TirId type) {
    fprintf(ctx->stream, "(");
    FunctionType func_type = get_function_type(ctx->tir, type);

    if (func_type.ret.id != TYPE_VOID && is_aggregate_type(ctx->tir, func_type.ret)) {
        fprintf(ctx->stream, "ptr");
        ctx->tmp_count++;
        if (func_type.param_count != 0) {
            fprintf(ctx->stream, ", ");
        }
    }

    for (int32_t i = 0; i < func_type.param_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }

        ctx->tmp_count++;
        TirId param_type = get_function_type_param(ctx->tir, type, i);
        if (is_aggregate_type(ctx->tir, param_type)) {
            fprintf(ctx->stream, "ptr");
        } else {
            gen_type(ctx, param_type);
        }
        fprintf(ctx->stream, " %%v%d", i);
    }

    fprintf(ctx->stream, ")");
}

static void gen_ret_type(GenContext *ctx, TirId type) {
    if (is_aggregate_type(ctx->tir, type)) {
        fprintf(ctx->stream, "ptr");
        return;
    }
    gen_type(ctx, type);
}

static void gen_extern_var(GenContext *ctx, TirId value) {
    TirId type = get_value_type(ctx->tir, value);
    char const *name = get_value_str(ctx->tir, value);
    fprintf(ctx->stream, "@%s = external global ", name);
    gen_type(ctx, type);
    fprintf(ctx->stream, ", align %d\n", alignof_type(ctx->tir, type, ctx->target));
}

static void gen_extern_function(GenContext *ctx, TirId value) {
    TirId type = get_value_type(ctx->tir, value);
    TirId ret_type = get_function_type(ctx->tir, type).ret;
    fprintf(ctx->stream, "declare ");
    gen_ret_type(ctx, ret_type);
    char const *name = get_value_str(ctx->tir, value);
    fprintf(ctx->stream, " @%s", name);
    gen_params(ctx, type);
    fprintf(ctx->stream, "\n");
}

static void gen_string(GenContext *ctx, int32_t index, char const *str) {
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    fprintf(ctx->stream, "@s%d = private unnamed_addr constant [%lu x i8] c\"", index, len + 1);
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] == '"' || str[i + 4] == '\\' || !(str[i + 4] >= 32 && str[i + 4] <= 126)) {
            fprintf(ctx->stream, "\\%02X", (int) (unsigned char) str[i + 4]);
        } else {
            fprintf(ctx->stream, "%c", str[i + 4]);
        }
    }
    fprintf(ctx->stream, "\\00\", align 1\n");
}

static void gen_value(GenContext *ctx, TirId value) {
    TirTag tag = get_term_tag(ctx->tir, value);
    switch (tag) {
        default: {
            abort();
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR: {
            char const *name = get_value_str(ctx->tir, value);
            fprintf(ctx->stream, "@%s", name);
            break;
        }
        case TIR_STRING: {
            fprintf(ctx->stream, "@s%d", ctx->strings.len);
            vec_push(&ctx->strings, get_value_str(ctx->tir, value));
            break;
        }
        case TIR_CONST_INT: {
            fprintf(ctx->stream, "%ld", get_value_int(ctx->tir, value));
            break;
        }
        case TIR_CONST_FLOAT: {
            int64_t bytes = sizeof_type(ctx->tir, get_value_type(ctx->tir, value), ctx->target);
            uint64_t raw = 0;
            if (bytes == 8) {
                raw = get_value_int(ctx->tir, value);
            } else if (bytes == 4) {
                union {
                    double value;
                    uint64_t bits;
                } f;
                f.value = (double) (float) get_value_float(ctx->tir, value);
                raw = f.bits;
            } else {
                abort();
            }
            char digits[17];
            for (int i = 0; i < 16; i++) {
                uint64_t digit = (raw >> ((15 - i) * 4)) & 0xF;
                if (digit < 10) {
                    digits[i] = digit + '0';
                } else {
                    digits[i] = digit - 10 + 'A';
                }
            }
            digits[16] = '\0';
            fprintf(ctx->stream, "0x%s", digits);
            break;
        }
        case TIR_CONST_NULL: {
            fprintf(ctx->stream, "null");
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            fprintf(ctx->stream, "%%v%d", get_term_data(ctx->tir, value)->b);
            break;
        }
    }
}

static Operand pop_operand(GenContext *ctx) {
    assert(ctx->stack.len >= 1);
    return ctx->stack.ptr[--ctx->stack.len];
}

static int32_t pop_data(GenContext *ctx) {
    return ctx->mir->data.ptr[ctx->data_top++];
}

static TirId pop_term(GenContext *ctx) {
    return (TirId) {pop_data(ctx)};
}

static Operand new_tmp(GenContext *ctx, bool is_lvalue, TirId type) {
    Operand operand = {
        .is_lvalue = is_lvalue,
        .tag = OPERAND_TMP,
        .type = type,
        .index = ctx->tmp_count++,
    };
    vec_push(&ctx->stack, operand);
    return operand;
}

static void gen_operand(GenContext *ctx, Operand *a) {
    switch (a->tag) {
        case OPERAND_INT: {
            fprintf(ctx->stream, "%ld", a->i);
            break;
        }
        case OPERAND_TIR: {
            gen_value(ctx, a->value);
            break;
        }
        default: {
            fprintf(ctx->stream, "%%%d", a->index);
            break;
        }
    }
}

static Operand load_operand(GenContext *ctx, Operand *a) {
    if (a->is_lvalue) {
        int32_t tmp = ctx->tmp_count++;
        fprintf(ctx->stream, "  %%%d = load ", tmp);
        gen_type(ctx, a->type);
        fprintf(ctx->stream, ", ptr ");
        gen_operand(ctx, a);
        fprintf(ctx->stream, "\n");
        return (Operand) {
            .is_lvalue = false,
            .tag = OPERAND_TMP,
            .type = a->type,
            .index = tmp,
        };
    }
    return *a;
}

static void gen_alloc(GenContext *ctx) {
    TirId local_type = pop_term(ctx);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    fprintf(ctx->stream, "  %%%d = alloca ", ctx->tmp_count++);
    gen_type(ctx, local_type);
    fprintf(ctx->stream, "\n");
}

static void gen_alloc_var(GenContext *ctx) {
    TirId v = pop_term(ctx);
    TirId local_type = get_value_type(ctx->tir, v);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    fprintf(ctx->stream, "  %%v%d = alloca ", get_term_data(ctx->tir, v)->b);
    gen_type(ctx, local_type);
    fprintf(ctx->stream, "\n");
}

static void gen_call_alloc(GenContext *ctx) {
    TirId type = pop_term(ctx);
    FunctionType function_type = get_function_type(ctx->tir, type);
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(ctx->tir, function_type.ret);
    if (function_type.ret.id != TYPE_VOID && implicit_return) {
        fprintf(ctx->stream, "  %%%d = alloca ", ctx->tmp_count++);
        gen_type(ctx, function_type.ret);
        fprintf(ctx->stream, "\n");
    }
}

static void gen_alloc2(GenContext *ctx) {
    TirId local_type = pop_term(ctx);
    Operand operand = {
        .is_lvalue = true,
        .tag = OPERAND_TMP,
        .type = local_type,
        .index = ctx->alloc_count++,
    };
    vec_push(&ctx->stack, operand);
}

static void gen_alloc_var2(GenContext *ctx) {
    TirId v = pop_term(ctx);
    TirId local_type = get_value_type(ctx->tir, v);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    Operand operand = {
        .is_lvalue = true,
        .tag = OPERAND_TIR,
        .type = local_type,
        .value = v,
    };
    vec_push(&ctx->stack, operand);
}

static void gen_neg(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    char const *op = "sub";
    char const *zero = "0";
    if (type_is_float(a.type)) {
        op = "fsub";
        zero = "0.0";
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, a.type).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " %s, ", zero);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, "\n");
}

static void gen_not(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    fprintf(ctx->stream, "  %%%d = xor ", new_tmp(ctx, false, a.type).index);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " -1, ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, "\n");
}

static void gen_binary(GenContext *ctx, char const *op) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    b = load_operand(ctx, &b);
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, a.type).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ", ");
    gen_operand(ctx, &b);
    fprintf(ctx->stream, "\n");
}

static void gen_overloaded_binary(
    GenContext *ctx,
    char const *op,
    char const *float_op
) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    b = load_operand(ctx, &b);
    if (type_is_float(a.type)) {
        op = float_op;
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, a.type).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ", ");
    gen_operand(ctx, &b);
    fprintf(ctx->stream, "\n");
}

static void gen_overloaded_cmp(
    GenContext *ctx,
    char const *op,
    char const *float_op
) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    b = load_operand(ctx, &b);
    if (type_is_float(a.type)) {
        op = float_op;
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, ptype(bool)).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ", ");
    gen_operand(ctx, &b);
    fprintf(ctx->stream, "\n");
}

static void stack_copy(GenContext *ctx) {
    assert(ctx->stack.len >= 1);
    Operand a = ctx->stack.ptr[ctx->stack.len - 1];
    vec_push(&ctx->stack, a);
}

static void stack_copy_at(GenContext *ctx) {
    int32_t index = pop_data(ctx);
    assert(ctx->stack.len + index >= 0);
    Operand a = ctx->stack.ptr[ctx->stack.len + index];
    vec_push(&ctx->stack, a);
}

static void stack_pop(GenContext *ctx) {
    ctx->stack.len -= 1;
}

static void gen_int(GenContext *ctx) {
    int32_t a = pop_data(ctx);
    int32_t b = pop_data(ctx);
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_INT,
        .type = ptype(i64),
        .i = load_i64(a, b),
    };
    vec_push(&ctx->stack, operand);
}

static void gen_tir_value(GenContext *ctx) {
    TirId a = pop_term(ctx);
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_TIR,
        .type = get_value_type(ctx->tir, a),
        .value = a,
    };
    switch (get_term_tag(ctx->tir, a)) {
        case TIR_EXTERN_VAR:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
        case TIR_STRING: {
            operand.is_lvalue = true;
            break;
        }
        case TIR_PARAMETER: {
            operand.is_lvalue = is_aggregate_type(ctx->tir, get_value_type(ctx->tir, a));
            break;
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL: {
            operand.is_lvalue = false;
            break;
        }
        default: {
            abort();
        }
    }
    vec_push(&ctx->stack, operand);
}

static void gen_address(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    TirId type = pop_term(ctx);
    assert(a.is_lvalue);
    a.is_lvalue = false;
    a.type = type;
    vec_push(&ctx->stack, a);
}

static void gen_deref(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);
    a.is_lvalue = true;
    a.type = remove_any_pointer(ctx->tir, a.type);
    vec_push(&ctx->stack, a);
}

static void gen_assign(GenContext *ctx) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    b = load_operand(ctx, &b);
    fprintf(ctx->stream, "  store ");
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &b);
    fprintf(ctx->stream, ", ptr ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, "\n");
}

static void gen_cast(GenContext *ctx, char const *op) {
    Operand a = pop_operand(ctx);
    TirId type = pop_term(ctx);
    a = load_operand(ctx, &a);
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, type).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, " to ");
    gen_type(ctx, type);
    fprintf(ctx->stream, "\n");
}

static void gen_cast_resize(GenContext *ctx, char const *op) {
    Operand a = pop_operand(ctx);
    TirId type = pop_term(ctx);
    a = load_operand(ctx, &a);
    if (sizeof_type(ctx->tir, a.type, ctx->target) == sizeof_type(ctx->tir, type, ctx->target)) {
        a.type = type;
        vec_push(&ctx->stack, a);
        return;
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, false, type).index, op);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, " to ");
    gen_type(ctx, type);
    fprintf(ctx->stream, "\n");
}

static void gen_call(GenContext *ctx) {
    TirId type = pop_term(ctx);
    FunctionType function_type = get_function_type(ctx->tir, type);
    int32_t arg_count = function_type.param_count;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(ctx->tir, function_type.ret);

    int32_t stack_elems = 1 + arg_count;
    int32_t f_index = ctx->stack.len - stack_elems;
    Operand f = load_operand(ctx, &ctx->stack.ptr[f_index]);

    for (int32_t i = 0; i < arg_count; i++) {
        if (!is_aggregate_type(ctx->tir, get_function_type_param(ctx->tir, type, i))) {
            ctx->stack.ptr[f_index + 1 + i] = load_operand(ctx, &ctx->stack.ptr[f_index + 1 + i]);
        }
    }

    Operand a;
    fprintf(ctx->stream, "  ");
    if (function_type.ret.id != TYPE_VOID) {
        a = new_tmp(ctx, implicit_return, function_type.ret);
        fprintf(ctx->stream, "%%%d = ", a.index);
        stack_elems++;
        if (implicit_return) {
            a = (Operand) {
                .is_lvalue = true,
                .tag = OPERAND_TMP,
                .type = a.type,
                .index =  ctx->alloc_count++,
            };
        }
    }

    fprintf(ctx->stream, "call ");
    if (implicit_return) {
        fprintf(ctx->stream, "ptr");
    } else {
        gen_type(ctx, function_type.ret);
    }
    fprintf(ctx->stream, " ");
    gen_operand(ctx, &f);
    fprintf(ctx->stream, "(");

    if (implicit_return) {

        fprintf(ctx->stream, "ptr %%%d", a.index);
        if (arg_count) {
            fprintf(ctx->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }

        if (is_aggregate_type(ctx->tir, ctx->stack.ptr[f_index + 1 + i].type)) {
            fprintf(ctx->stream, "ptr ");
        } else {
            gen_type(ctx, ctx->stack.ptr[f_index + 1 + i].type);
            fprintf(ctx->stream, " ");
        }

        gen_operand(ctx, &ctx->stack.ptr[f_index + 1 + i]);
    }

    fprintf(ctx->stream, ")\n");
    assert(ctx->stack.len >= stack_elems);
    ctx->stack.len -= stack_elems;

    if (function_type.ret.id != TYPE_VOID) {
        vec_push(&ctx->stack, a);
    }
}

static void gen_index(GenContext *ctx) {
    Operand index = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    index = load_operand(ctx, &index);
    int32_t tmp = new_tmp(ctx, true, remove_c_pointer_like(ctx->tir, a.type)).index;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", tmp);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, ", ptr ");
    assert(a.is_lvalue);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ", i64 0, i64 ");
    gen_operand(ctx, &index);
    fprintf(ctx->stream, "\n");
}

static void gen_slice_index(GenContext *ctx) {
    Operand index = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    index = load_operand(ctx, &index);

    int32_t tmp1 = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", tmp1);
    gen_type(ctx, a.type);
    fprintf(ctx->stream, ", ptr ");
    assert(a.is_lvalue);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ", i64 0, i32 1\n");

    int32_t tmp2 = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = load ptr, ptr %%%d\n", tmp2, tmp1);

    TirId elem_type = remove_c_pointer_like(ctx->tir, a.type);
    int32_t tmp3 = new_tmp(ctx, true, elem_type).index;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", tmp3);
    gen_type(ctx, elem_type);
    fprintf(ctx->stream, ", ptr %%%d, i64 ", tmp2);
    gen_operand(ctx, &index);
    fprintf(ctx->stream, "\n");
}

static void gen_access(GenContext *ctx) {
    Operand s = pop_operand(ctx);
    int32_t field = pop_data(ctx);
    TirId field_type = get_any_struct_type_field(ctx->tir, s.type, field);
    int32_t tmp = new_tmp(ctx, true, field_type).index;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", tmp);
    gen_type(ctx, s.type);
    fprintf(ctx->stream, ", ptr ");
    assert(s.is_lvalue);
    gen_operand(ctx, &s);
    fprintf(ctx->stream, ", i64 0, i32 %d\n", field);
}

static void gen_br(GenContext *ctx) {
    int32_t block = pop_data(ctx);
    fprintf(ctx->stream, "  br label %%L.%d\n", block);
}

static void gen_br_if(GenContext *ctx) {
    Operand condition = pop_operand(ctx);
    int32_t block = pop_data(ctx);
    condition = load_operand(ctx, &condition);
    fprintf(ctx->stream, "  br i1 ");
    gen_operand(ctx, &condition);
    fprintf(ctx->stream, ", label %%L.%d, label %%L.%d\n", block, ctx->blocks);
}

static void gen_br_if_not(GenContext *ctx) {
    Operand condition = pop_operand(ctx);
    int32_t block = pop_data(ctx);
    condition = load_operand(ctx, &condition);
    fprintf(ctx->stream, "  br i1 ");
    gen_operand(ctx, &condition);
    fprintf(ctx->stream, ", label %%L.%d, label %%L.%d\n", ctx->blocks, block);
}

static void gen_ret_void(GenContext *ctx) {
    if (ctx->is_main) {
        fprintf(ctx->stream, "  ret i32 0\n");
    } else {
        fprintf(ctx->stream, "  ret void\n");
    }
}

static void gen_ret(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    a = load_operand(ctx, &a);

    if (!is_aggregate_type(ctx->tir, a.type)) {
        fprintf(ctx->stream, "  ret ");
        gen_type(ctx, a.type);
        fprintf(ctx->stream, " ");
        gen_operand(ctx, &a);
        fprintf(ctx->stream, "\n");
    } else {
        fprintf(ctx->stream, "  store ");
        gen_type(ctx, a.type);
        fprintf(ctx->stream, " ");
        gen_operand(ctx, &a);
        fprintf(ctx->stream, ", ptr %%0\n");

        fprintf(ctx->stream, "  ret ptr %%0\n");
    }
}

static void gen_instruction(GenContext *ctx, MirTag tag) {
    switch (tag) {
        case MIR_STACK_COPY: stack_copy(ctx); break;
        case MIR_STACK_COPY_AT: stack_copy_at(ctx); break;
        case MIR_STACK_POP: stack_pop(ctx); break;
        case MIR_ALLOC: gen_alloc2(ctx); break;
        case MIR_ALLOC_VAR: gen_alloc_var2(ctx); break;
        case MIR_INT: gen_int(ctx); break;
        case MIR_TIR_VALUE: gen_tir_value(ctx); break;
        case MIR_ADDRESS: gen_address(ctx); break;
        case MIR_DEREF: gen_deref(ctx); break;
        case MIR_ASSIGN: gen_assign(ctx); break;
        case MIR_NEG: gen_neg(ctx); break;
        case MIR_NOT: gen_not(ctx); break;
        case MIR_ADD: gen_overloaded_binary(ctx, "add", "fadd"); break;
        case MIR_SUB: gen_overloaded_binary(ctx, "sub", "fsub"); break;
        case MIR_MUL: gen_overloaded_binary(ctx, "mul", "fmul"); break;
        case MIR_DIV: gen_overloaded_binary(ctx, "sdiv", "fdiv"); break;
        case MIR_MOD: gen_overloaded_binary(ctx, "srem", "frem"); break;
        case MIR_AND: gen_binary(ctx, "and"); break;
        case MIR_OR: gen_binary(ctx, "or"); break;
        case MIR_XOR: gen_binary(ctx, "xor"); break;
        case MIR_SHL: gen_binary(ctx, "shl"); break;
        case MIR_SHR: gen_binary(ctx, "ashr"); break;
        case MIR_EQ: gen_overloaded_cmp(ctx, "icmp eq", "fcmp eq"); break;
        case MIR_NE: gen_overloaded_cmp(ctx, "icmp ne", "fcmp ne"); break;
        case MIR_LT: gen_overloaded_cmp(ctx, "icmp slt", "fcmp lt"); break;
        case MIR_GT: gen_overloaded_cmp(ctx, "icmp sgt", "fcmp gt"); break;
        case MIR_LE: gen_overloaded_cmp(ctx, "icmp sle", "fcmp le"); break;
        case MIR_GE: gen_overloaded_cmp(ctx, "icmp sge", "fcmp ge"); break;

        case MIR_ITOF: gen_cast(ctx, "sitofp"); break;
        case MIR_ITRUNC: gen_cast_resize(ctx, "trunc"); break;
        case MIR_SEXT: gen_cast_resize(ctx, "sext"); break;
        case MIR_ZEXT: gen_cast_resize(ctx, "zext"); break;
        case MIR_FTOI: gen_cast(ctx, "fptosi"); break;
        case MIR_FTRUNC: gen_cast_resize(ctx, "fptrunc"); break;
        case MIR_FEXT: gen_cast_resize(ctx, "fpext"); break;
        case MIR_PTR_CAST: gen_cast_resize(ctx, "bitcast"); break;

        case MIR_CALL: gen_call(ctx); break;
        case MIR_INDEX: gen_index(ctx); break;
        case MIR_SLICE_INDEX: gen_slice_index(ctx); break;
        case MIR_ACCESS: gen_access(ctx); break;
        case MIR_BR: gen_br(ctx); break;
        case MIR_BR_IF: gen_br_if(ctx); break;
        case MIR_BR_IF_NOT: gen_br_if_not(ctx); break;
        case MIR_RET_VOID: gen_ret_void(ctx); break;
        case MIR_RET: gen_ret(ctx); break;
    }
}

static void gen_function(GenContext *ctx, GenInput *input, int32_t f_index) {
    TirId value = input->global_deps.functions.ptr[f_index];
    int32_t mir_start = input->mir_result->ends[f_index];
    int32_t mir_end = input->mir_result->ends[f_index + 1];
    int32_t data_start = input->mir_result->data_starts[f_index];
    bool is_main = input->global_deps.main.id == value.id;

    ctx->tir.thread = &input->insts[f_index].deps;
    ctx->tmp_count = 0;
    ctx->alloc_count = 0;
    ctx->stack.len = 0;
    TirId type = get_value_type(ctx->tir, value);
    TirId ret_type = get_function_type(ctx->tir, type).ret;
    ctx->is_main = is_main;
    if (is_main) {
        fprintf(ctx->stream, "define i32 @main");
    } else {
        fprintf(ctx->stream, "define private ");
        gen_ret_type(ctx, ret_type);
        char const *name = get_value_str(ctx->tir, value);
        fprintf(ctx->stream, " @%s", name);
    }
    gen_params(ctx, type);
    fprintf(ctx->stream, " {\n");
    ctx->blocks = 1;
    ctx->tmp_count++;
    ctx->alloc_count = ctx->tmp_count;
    ctx->data_top = data_start;
    for (int32_t i = mir_start; i < mir_end; i++) {
        switch ((MirTag) ctx->mir->insts.ptr[i]) {
            case MIR_ALLOC: gen_alloc(ctx); break;
            case MIR_ALLOC_VAR: gen_alloc_var(ctx); break;
            case MIR_CALL: gen_call_alloc(ctx); break;

            case MIR_STACK_COPY:
            case MIR_STACK_POP:
            case MIR_NEG:
            case MIR_NOT:
            case MIR_DEREF:
            case MIR_ASSIGN:
            case MIR_ADD:
            case MIR_SUB:
            case MIR_MUL:
            case MIR_DIV:
            case MIR_MOD:
            case MIR_AND:
            case MIR_OR:
            case MIR_XOR:
            case MIR_SHL:
            case MIR_SHR:
            case MIR_EQ:
            case MIR_NE:
            case MIR_LT:
            case MIR_GT:
            case MIR_LE:
            case MIR_GE:
            case MIR_INDEX:
            case MIR_SLICE_INDEX:
            case MIR_RET_VOID:
            case MIR_RET: {
                break;
            }
            case MIR_STACK_COPY_AT:
            case MIR_TIR_VALUE:
            case MIR_ITOF:
            case MIR_ITRUNC:
            case MIR_SEXT:
            case MIR_ZEXT:
            case MIR_FTOI:
            case MIR_FTRUNC:
            case MIR_FEXT:
            case MIR_PTR_CAST:
            case MIR_ACCESS:
            case MIR_ADDRESS:
            case MIR_BR:
            case MIR_BR_IF:
            case MIR_BR_IF_NOT: {
                ctx->data_top += 1;
                break;
            }
            case MIR_INT: {
                ctx->data_top += 2;
                break;
            }
        }
    }
    ctx->data_top = data_start;
    for (int32_t i = mir_start; i < mir_end; i++) {
        if (i != mir_start && is_mir_terminator(ctx->mir->insts.ptr[i - 1])) {
            fprintf(ctx->stream, "L.%d:\n", ctx->blocks++);
        }
        gen_instruction(ctx, ctx->mir->insts.ptr[i]);
    }

    fprintf(ctx->stream, "}\n");
    assert(ctx->stack.len == 0);
    assert(
        f_index == input->global_deps.functions.len - 1
            ? (ctx->data_top == input->mir_result->mir.data.len)
            : (ctx->data_top == input->mir_result->data_starts[f_index + 1])
    );
}

static void gen_struct(GenContext *ctx, TirId type) {
    TaggedType t = get_tagged_type(ctx->tir, type);
    StructType s = get_struct_type(ctx->tir, t.inner);
    fprintf(ctx->stream, "%%_%s = type { ", tir_get_str(ctx->tir, t.name));

    for (int32_t i = 0; i < s.field_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }
        TirId field_type = s.fields[i];
        gen_type(ctx, field_type);
    }

    fprintf(ctx->stream, " }\n");
}

void gen_llvm(GenInput *input, Target target) {
    FILE *stream = fopen("a.ll", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "%%slice = type { i%d, ptr }\n", (int) sizeof_pointer(target) * 8);

    GenContext ctx = {
        .target = target,
        .tir = {
            .global = &input->global_deps,
            .thread = NULL,
        },
        .mir = &input->mir_result->mir,
        .stream = stream,
    };

    for (int32_t i = 0; i < input->global_deps.structs.len; i++) {
        TirId type = input->global_deps.structs.ptr[i];
        gen_struct(&ctx, type);
    }

    for (int32_t i = 0; i < input->global_deps.extern_vars.len; i++) {
        TirId value = input->global_deps.extern_vars.ptr[i];
        gen_extern_var(&ctx, value);
    }

    for (int32_t i = 0; i < input->global_deps.extern_functions.len; i++) {
        TirId value = input->global_deps.extern_functions.ptr[i];
        gen_extern_function(&ctx, value);
    }

    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_function(&ctx, input, i);
    }

    for (int32_t i = 0; i < ctx.strings.len; i++) {
        gen_string(&ctx, i, ctx.strings.ptr[i]);
    }

    fclose(stream);
}
