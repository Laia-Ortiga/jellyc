#include "gen.h"

#include "adt.h"
#include "mir.h"
#include "tir.h"
#include "type.h"
#include "fwd.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
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
    bool has_overflow_block;
    bool is_main;
    Target target;
    TirContext tir;
    int32_t thread;
    FILE *stream;
} GenContext;

static void gen_struct_name(GenContext *c, TirId type) {
    TirStructType s = tir_get_struct_type(c->tir, type);
    if (tir_get_storage(c->tir, type) == c->tir.global) {
        fprintf(c->stream, "%%_%d_%d_%s", 0, type.id, tir_get_str(c->tir, s.name));
    } else {
        fprintf(c->stream, "%%_%d_%d_%s", c->thread, type.id, tir_get_str(c->tir, s.name));
    }
}

static void gen_type(GenContext *c, TirId type) {
    switch (get_tir_tag(c->tir, type)) {
        case TIR_RESERVED: {
            switch ((ReservedTerm) type.id) {
                case TYPE_VOID: fprintf(c->stream, "void"); return;

                case TYPE_i8:
                case TYPE_byte: fprintf(c->stream, "i8"); return;

                case TYPE_i16: fprintf(c->stream, "i16"); return;
                case TYPE_i32: fprintf(c->stream, "i32"); return;
                case TYPE_i64: fprintf(c->stream, "i64"); return;

                case TYPE_isize: fprintf(c->stream, "i%d", sizeof_pointer(c->target) * 8); return;

                case TYPE_f32: fprintf(c->stream, "float"); return;
                case TYPE_f64: fprintf(c->stream, "double"); return;
                case TYPE_bool: fprintf(c->stream, "i1"); return;

                default: break;
            }
        }
        case TIR_TYPE_PARAMETER: {
            break;
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType array = tir_get_array_type(c->tir, type);
            int64_t length = tir_get_array_length_type(c->tir, array.index).length;
            fprintf(c->stream, "[%ld x ", length);
            gen_type(c, array.elem);
            fprintf(c->stream, "]");
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            fprintf(c->stream, "i%d", sizeof_pointer(c->target) * 8);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE: {
            fprintf(c->stream, "ptr");
            return;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            fprintf(c->stream, "%%slice");
            return;
        }
        case TIR_STRUCT_TYPE: {
            gen_struct_name(c, type);
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type(c, tir_get_enum_type(c->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type(c, tir_get_tagged_type(c->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type(c, tir_get_affine_type(c->tir, type).elem);
            return;
        }
        default: {
            abort();
        }
    }
}

static void gen_params(GenContext *c, TirId type) {
    fprintf(c->stream, "(");
    TirFunctionType func_type = tir_get_function_type(c->tir, type);

    if (func_type.ret.id != TYPE_VOID && is_aggregate_type(c->tir, func_type.ret)) {
        fprintf(c->stream, "ptr");
        c->tmp_count++;
        if (func_type.params.len != 0) {
            fprintf(c->stream, ", ");
        }
    }

    for (int32_t i = 0; i < func_type.params.len; i++) {
        if (i != 0) {
            fprintf(c->stream, ", ");
        }

        c->tmp_count++;
        TirId param_type = get_function_type_param(c->tir, type, i);
        if (is_aggregate_type(c->tir, param_type)) {
            fprintf(c->stream, "ptr");
        } else {
            gen_type(c, param_type);
        }
        fprintf(c->stream, " %%v%d", i);
    }

    fprintf(c->stream, ")");
}

static void gen_ret_type(GenContext *c, TirId type) {
    if (is_aggregate_type(c->tir, type)) {
        fprintf(c->stream, "ptr");
        return;
    }
    gen_type(c, type);
}

static void gen_extern_var(GenContext *c, TirId value) {
    TirExternVar t = tir_get_extern_var(c->tir, value);
    char const *name = tir_get_str(c->tir, t.name);
    fprintf(c->stream, "@%s = external global ", name);
    gen_type(c, t.type);
    fprintf(c->stream, ", align %d\n", alignof_type(c->tir, t.type, c->target));
}

static void gen_extern_function(GenContext *c, TirId value) {
    TirExternFunction t = tir_get_extern_function(c->tir, value);
    TirId ret_type = tir_get_function_type(c->tir, t.type).ret;
    fprintf(c->stream, "declare ");
    gen_ret_type(c, ret_type);
    char const *name = tir_get_str(c->tir, t.name);
    fprintf(c->stream, " @%s", name);
    gen_params(c, t.type);
    fprintf(c->stream, "\n");
}

static void gen_string(GenContext *c, int32_t index, char const *str) {
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    fprintf(c->stream, "@s%d = private unnamed_addr constant [%lu x i8] c\"", index, len + 1);
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] == '"' || str[i + 4] == '\\' || !(str[i + 4] >= 32 && str[i + 4] <= 126)) {
            fprintf(c->stream, "\\%02X", (int) (unsigned char) str[i + 4]);
        } else {
            fprintf(c->stream, "%c", str[i + 4]);
        }
    }
    fprintf(c->stream, "\\00\", align 1\n");
}

static void gen_value(GenContext *c, TirId value) {
    TirTag tag = get_tir_tag(c->tir, value);
    switch (tag) {
        default: {
            abort();
        }
        case TIR_FUNCTION: {
            TirFunction t = tir_get_function(c->tir, value);
            fprintf(c->stream, "@%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_EXTERN_FUNCTION: {
            TirExternFunction t = tir_get_extern_function(c->tir, value);
            fprintf(c->stream, "@%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_EXTERN_VAR: {
            TirExternVar t = tir_get_extern_var(c->tir, value);
            fprintf(c->stream, "@%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_STRING: {
            fprintf(c->stream, "@s%d", c->strings.len);
            TirString t = tir_get_string(c->tir, value);
            vec_push(&c->strings, tir_get_str(c->tir, t.value));
            break;
        }
        case TIR_INT: {
            fprintf(c->stream, "%ld", tir_get_int(c->tir, value).value);
            break;
        }
        case TIR_FLOAT: {
            union {
                double value;
                uint64_t bits;
            } f;
            f.value = tir_get_float(c->tir, value).value;
            switch (sizeof_type(c->tir, get_value_type(c->tir, value), c->target)) {
                case 8: {
                    break;
                }
                case 4: {
                    f.value = (double) (float) f.value;
                    break;
                }
                default: {
                    abort();
                }
            }
            char digits[17];
            for (int i = 0; i < 16; i++) {
                uint64_t digit = (f.bits >> ((15 - i) * 4)) & 0xF;
                if (digit < 10) {
                    digits[i] = digit + '0';
                } else {
                    digits[i] = digit - 10 + 'A';
                }
            }
            digits[16] = '\0';
            fprintf(c->stream, "0x%s", digits);
            break;
        }
        case TIR_NULL: {
            fprintf(c->stream, "null");
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            fprintf(c->stream, "%%v%d", tir_get_variable(c->tir, value).index);
            break;
        }
    }
}

static Operand pop_operand(GenContext *c) {
    assert(c->stack.len >= 1);
    return c->stack.ptr[--c->stack.len];
}

static int32_t pop_data(GenContext *c) {
    return c->mir->data.ptr[c->data_top++];
}

static TirId pop_term(GenContext *c) {
    return (TirId) {pop_data(c)};
}

static Operand new_tmp2(GenContext *c, bool is_lvalue, TirId type) {
    Operand operand = {
        .is_lvalue = is_lvalue,
        .tag = OPERAND_TMP,
        .type = type,
        .index = c->tmp_count++,
    };
    return operand;
}

static Operand new_tmp(GenContext *c, bool is_lvalue, TirId type) {
    Operand o = new_tmp2(c, is_lvalue, type);
    vec_push(&c->stack, o);
    return o;
}

static void gen_operand(GenContext *c, Operand *a) {
    switch (a->tag) {
        case OPERAND_INT: {
            fprintf(c->stream, "%ld", a->i);
            break;
        }
        case OPERAND_TIR: {
            gen_value(c, a->value);
            break;
        }
        default: {
            fprintf(c->stream, "%%%d", a->index);
            break;
        }
    }
}

static Operand load_operand(GenContext *c, Operand *a) {
    if (a->is_lvalue) {
        int32_t tmp = c->tmp_count++;
        fprintf(c->stream, "  %%%d = load ", tmp);
        gen_type(c, a->type);
        fprintf(c->stream, ", ptr ");
        gen_operand(c, a);
        fprintf(c->stream, "\n");
        return (Operand) {
            .is_lvalue = false,
            .tag = OPERAND_TMP,
            .type = a->type,
            .index = tmp,
        };
    }
    return *a;
}

static void gen_alloc(GenContext *c) {
    TirId local_type = pop_term(c);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    fprintf(c->stream, "  %%%d = alloca ", c->tmp_count++);
    gen_type(c, local_type);
    fprintf(c->stream, "\n");
}

static void gen_alloc_var(GenContext *c) {
    TirId v = pop_term(c);
    TirId local_type = get_value_type(c->tir, v);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    fprintf(c->stream, "  %%v%d = alloca ", tir_get_variable(c->tir, v).index);
    gen_type(c, local_type);
    fprintf(c->stream, "\n");
}

static void gen_call_alloc(GenContext *c) {
    TirId type = pop_term(c);
    TirFunctionType function_type = tir_get_function_type(c->tir, type);
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(c->tir, function_type.ret);
    if (function_type.ret.id != TYPE_VOID && implicit_return) {
        fprintf(c->stream, "  %%%d = alloca ", c->tmp_count++);
        gen_type(c, function_type.ret);
        fprintf(c->stream, "\n");
    }
}

static void gen_alloc2(GenContext *c) {
    TirId local_type = pop_term(c);
    Operand operand = {
        .is_lvalue = true,
        .tag = OPERAND_TMP,
        .type = local_type,
        .index = c->alloc_count++,
    };
    vec_push(&c->stack, operand);
}

static void gen_alloc_var2(GenContext *c) {
    TirId v = pop_term(c);
    TirId local_type = get_value_type(c->tir, v);
    if (local_type.id == TYPE_VOID) {
        abort();
    }
    Operand operand = {
        .is_lvalue = true,
        .tag = OPERAND_TIR,
        .type = local_type,
        .value = v,
    };
    vec_push(&c->stack, operand);
}

static void gen_neg(GenContext *c) {
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    char const *op = "sub";
    char const *zero = "0";
    if (type_is_float(a.type)) {
        op = "fsub";
        zero = "0.0";
    }
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, a.type).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " %s, ", zero);
    gen_operand(c, &a);
    fprintf(c->stream, "\n");
}

static void gen_not(GenContext *c) {
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    fprintf(c->stream, "  %%%d = xor ", new_tmp(c, false, a.type).index);
    gen_type(c, a.type);
    fprintf(c->stream, " -1, ");
    gen_operand(c, &a);
    fprintf(c->stream, "\n");
}

static void gen_binary(GenContext *c, char const *op) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, a.type).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_operand(c, &b);
    fprintf(c->stream, "\n");
}

static void gen_overloaded_binary(
    GenContext *c,
    char const *op,
    char const *float_op
) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (type_is_float(a.type)) {
        op = float_op;
    }
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, a.type).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_operand(c, &b);
    fprintf(c->stream, "\n");
}

static void gen_overloaded_cmp(
    GenContext *c,
    char const *op,
    char const *float_op
) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (type_is_float(a.type)) {
        op = float_op;
    }
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, ptype(bool)).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_operand(c, &b);
    fprintf(c->stream, "\n");
}

static void stack_copy(GenContext *c) {
    assert(c->stack.len >= 1);
    Operand a = c->stack.ptr[c->stack.len - 1];
    vec_push(&c->stack, a);
}

static void stack_copy_at(GenContext *c) {
    int32_t index = pop_data(c);
    assert(c->stack.len + index >= 0);
    Operand a = c->stack.ptr[c->stack.len + index];
    vec_push(&c->stack, a);
}

static void stack_pop(GenContext *c) {
    assert(c->stack.len >= 1);
    c->stack.len -= 1;
}

static void gen_int(GenContext *c) {
    int32_t a = pop_data(c);
    int32_t b = pop_data(c);
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_INT,
        .type = ptype(i64),
        .i = load_i64(a, b),
    };
    vec_push(&c->stack, operand);
}

static void gen_tir_value(GenContext *c) {
    TirId a = pop_term(c);
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_TIR,
        .type = get_value_type(c->tir, a),
        .value = a,
    };
    switch (get_tir_tag(c->tir, a)) {
        case TIR_EXTERN_VAR:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
        case TIR_STRING: {
            operand.is_lvalue = true;
            break;
        }
        case TIR_PARAMETER: {
            operand.is_lvalue = is_aggregate_type(c->tir, get_value_type(c->tir, a));
            break;
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_INT:
        case TIR_FLOAT:
        case TIR_NULL: {
            operand.is_lvalue = false;
            break;
        }
        default: {
            abort();
        }
    }
    vec_push(&c->stack, operand);
}

static void gen_address(GenContext *c) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    assert(a.is_lvalue);
    a.is_lvalue = false;
    a.type = type;
    vec_push(&c->stack, a);
}

static void gen_deref(GenContext *c) {
    Operand a = pop_operand(c);
    a = load_operand(c, &a);
    a.is_lvalue = true;
    a.type = remove_any_pointer(c->tir, a.type);
    vec_push(&c->stack, a);
}

static void gen_assign(GenContext *c) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    b = load_operand(c, &b);
    fprintf(c->stream, "  store ");
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &b);
    fprintf(c->stream, ", ptr ");
    gen_operand(c, &a);
    fprintf(c->stream, "\n");
}

static void gen_cast(GenContext *c, char const *op) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    a = load_operand(c, &a);
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, type).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, " to ");
    gen_type(c, type);
    fprintf(c->stream, "\n");
}

static void gen_narrow(GenContext *c) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    a = load_operand(c, &a);
    Operand result = new_tmp(c, false, type);
    fprintf(c->stream, "  %%%d = trunc ", result.index);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, " to ");
    gen_type(c, type);
    fprintf(c->stream, "\n");

    Operand b = new_tmp2(c, false, a.type);
    fprintf(c->stream, "  %%%d = sext ", b.index);
    gen_type(c, result.type);
    fprintf(c->stream, " ");
    gen_operand(c, &result);
    fprintf(c->stream, " to ");
    gen_type(c, b.type);
    fprintf(c->stream, "\n");

    Operand overflowed = new_tmp2(c, false, ptype(bool));
    fprintf(c->stream, "  %%%d = icmp ne ", overflowed.index);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_operand(c, &b);
    fprintf(c->stream, "\n");

    fprintf(c->stream, "  br i1 ");
    gen_operand(c, &overflowed);
    int32_t next_block = c->tmp_count++;
    fprintf(c->stream, ", label %%L.overflow, label %%%d\n", next_block);
    c->has_overflow_block = true;
}

static void gen_nop(GenContext *c) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    a.type = type;
    vec_push(&c->stack, a);
}

static void gen_call(GenContext *c) {
    TirId type = pop_term(c);
    TirFunctionType function_type = tir_get_function_type(c->tir, type);
    int32_t arg_count = function_type.params.len;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(c->tir, function_type.ret);

    int32_t stack_elems = 1 + arg_count;
    int32_t f_index = c->stack.len - stack_elems;
    Operand f = load_operand(c, &c->stack.ptr[f_index]);

    for (int32_t i = 0; i < arg_count; i++) {
        if (!is_aggregate_type(c->tir, get_function_type_param(c->tir, type, i))) {
            c->stack.ptr[f_index + 1 + i] = load_operand(c, &c->stack.ptr[f_index + 1 + i]);
        }
    }

    Operand a;
    fprintf(c->stream, "  ");
    if (function_type.ret.id != TYPE_VOID) {
        a = new_tmp(c, implicit_return, function_type.ret);
        fprintf(c->stream, "%%%d = ", a.index);
        stack_elems++;
        if (implicit_return) {
            a = (Operand) {
                .is_lvalue = true,
                .tag = OPERAND_TMP,
                .type = a.type,
                .index =  c->alloc_count++,
            };
        }
    }

    fprintf(c->stream, "call ");
    if (implicit_return) {
        fprintf(c->stream, "ptr");
    } else {
        gen_type(c, function_type.ret);
    }
    fprintf(c->stream, " ");
    gen_operand(c, &f);
    fprintf(c->stream, "(");

    if (implicit_return) {

        fprintf(c->stream, "ptr %%%d", a.index);
        if (arg_count) {
            fprintf(c->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(c->stream, ", ");
        }

        if (is_aggregate_type(c->tir, c->stack.ptr[f_index + 1 + i].type)) {
            fprintf(c->stream, "ptr ");
        } else {
            gen_type(c, c->stack.ptr[f_index + 1 + i].type);
            fprintf(c->stream, " ");
        }

        gen_operand(c, &c->stack.ptr[f_index + 1 + i]);
    }

    fprintf(c->stream, ")\n");
    assert(c->stack.len >= stack_elems);
    c->stack.len -= stack_elems;

    if (function_type.ret.id != TYPE_VOID) {
        vec_push(&c->stack, a);
    }
}

static void gen_index(GenContext *c) {
    Operand index = pop_operand(c);
    Operand a = pop_operand(c);
    index = load_operand(c, &index);
    int32_t tmp = new_tmp(c, true, remove_c_pointer_like(c->tir, a.type)).index;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp);
    gen_type(c, a.type);
    fprintf(c->stream, ", ptr ");
    assert(a.is_lvalue);
    gen_operand(c, &a);
    fprintf(c->stream, ", i64 0, i64 ");
    gen_operand(c, &index);
    fprintf(c->stream, "\n");
}

static void gen_slice_index(GenContext *c) {
    Operand index = pop_operand(c);
    Operand a = pop_operand(c);
    index = load_operand(c, &index);

    int32_t tmp1 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp1);
    gen_type(c, a.type);
    fprintf(c->stream, ", ptr ");
    assert(a.is_lvalue);
    gen_operand(c, &a);
    fprintf(c->stream, ", i64 0, i32 1\n");

    int32_t tmp2 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = load ptr, ptr %%%d\n", tmp2, tmp1);

    TirId elem_type = remove_c_pointer_like(c->tir, a.type);
    int32_t tmp3 = new_tmp(c, true, elem_type).index;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp3);
    gen_type(c, elem_type);
    fprintf(c->stream, ", ptr %%%d, i64 ", tmp2);
    gen_operand(c, &index);
    fprintf(c->stream, "\n");
}

static void gen_access(GenContext *c) {
    Operand s = pop_operand(c);
    int32_t field = pop_data(c);
    TirId field_type = get_struct_type_field(c->tir, s.type, field);
    int32_t tmp = new_tmp(c, true, field_type).index;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp);
    gen_type(c, s.type);
    fprintf(c->stream, ", ptr ");
    assert(s.is_lvalue);
    gen_operand(c, &s);
    fprintf(c->stream, ", i64 0, i32 %d\n", field);
}

static void gen_br(GenContext *c) {
    int32_t block = pop_data(c);
    fprintf(c->stream, "  br label %%L.%d\n", block);
}

static void gen_br_if(GenContext *c) {
    Operand condition = pop_operand(c);
    int32_t block = pop_data(c);
    condition = load_operand(c, &condition);
    fprintf(c->stream, "  br i1 ");
    gen_operand(c, &condition);
    fprintf(c->stream, ", label %%L.%d, label %%L.%d\n", block, c->blocks);
}

static void gen_br_if_not(GenContext *c) {
    Operand condition = pop_operand(c);
    int32_t block = pop_data(c);
    condition = load_operand(c, &condition);
    fprintf(c->stream, "  br i1 ");
    gen_operand(c, &condition);
    fprintf(c->stream, ", label %%L.%d, label %%L.%d\n", c->blocks, block);
}

static void gen_ret_void(GenContext *c) {
    if (c->is_main) {
        fprintf(c->stream, "  ret i32 0\n");
    } else {
        fprintf(c->stream, "  ret void\n");
    }
}

static void gen_ret(GenContext *c) {
    Operand a = pop_operand(c);
    a = load_operand(c, &a);

    if (!is_aggregate_type(c->tir, a.type)) {
        fprintf(c->stream, "  ret ");
        gen_type(c, a.type);
        fprintf(c->stream, " ");
        gen_operand(c, &a);
        fprintf(c->stream, "\n");
    } else {
        fprintf(c->stream, "  store ");
        gen_type(c, a.type);
        fprintf(c->stream, " ");
        gen_operand(c, &a);
        fprintf(c->stream, ", ptr %%0\n");

        fprintf(c->stream, "  ret ptr %%0\n");
    }
}

static void gen_instruction(GenContext *c, MirTag tag) {
    switch (tag) {
        case MIR_STACK_COPY: stack_copy(c); break;
        case MIR_STACK_COPY_AT: stack_copy_at(c); break;
        case MIR_STACK_POP: stack_pop(c); break;
        case MIR_ALLOC: gen_alloc2(c); break;
        case MIR_ALLOC_VAR: gen_alloc_var2(c); break;
        case MIR_INT: gen_int(c); break;
        case MIR_TIR_VALUE: gen_tir_value(c); break;
        case MIR_ADDRESS: gen_address(c); break;
        case MIR_DEREF: gen_deref(c); break;
        case MIR_ASSIGN: gen_assign(c); break;
        case MIR_NEG: gen_neg(c); break;
        case MIR_NOT: gen_not(c); break;
        case MIR_ADD: gen_overloaded_binary(c, "add", "fadd"); break;
        case MIR_SUB: gen_overloaded_binary(c, "sub", "fsub"); break;
        case MIR_MUL: gen_overloaded_binary(c, "mul", "fmul"); break;
        case MIR_DIV: gen_overloaded_binary(c, "sdiv", "fdiv"); break;
        case MIR_MOD: gen_overloaded_binary(c, "srem", "frem"); break;
        case MIR_AND: gen_binary(c, "and"); break;
        case MIR_OR: gen_binary(c, "or"); break;
        case MIR_XOR: gen_binary(c, "xor"); break;
        case MIR_SHL: gen_binary(c, "shl"); break;
        case MIR_SHR: gen_binary(c, "ashr"); break;
        case MIR_EQ: gen_overloaded_cmp(c, "icmp eq", "fcmp eq"); break;
        case MIR_NE: gen_overloaded_cmp(c, "icmp ne", "fcmp ne"); break;
        case MIR_LT: gen_overloaded_cmp(c, "icmp slt", "fcmp lt"); break;
        case MIR_GT: gen_overloaded_cmp(c, "icmp sgt", "fcmp gt"); break;
        case MIR_LE: gen_overloaded_cmp(c, "icmp sle", "fcmp le"); break;
        case MIR_GE: gen_overloaded_cmp(c, "icmp sge", "fcmp ge"); break;

        case MIR_ITOF: gen_cast(c, "sitofp"); break;
        case MIR_ITRUNC: gen_cast(c, "trunc"); break;
        case MIR_INARROW: gen_narrow(c); break;
        case MIR_SEXT: gen_cast(c, "sext"); break;
        case MIR_ZEXT: gen_cast(c, "zext"); break;
        case MIR_FTOI: gen_cast(c, "fptosi"); break;
        case MIR_FTRUNC: gen_cast(c, "fptrunc"); break;
        case MIR_FEXT: gen_cast(c, "fpext"); break;
        case MIR_NOP: gen_nop(c); break;

        case MIR_CALL: gen_call(c); break;
        case MIR_INDEX: gen_index(c); break;
        case MIR_SLICE_INDEX: gen_slice_index(c); break;
        case MIR_ACCESS: gen_access(c); break;
        case MIR_BR: gen_br(c); break;
        case MIR_BR_IF: gen_br_if(c); break;
        case MIR_BR_IF_NOT: gen_br_if_not(c); break;
        case MIR_RET_VOID: gen_ret_void(c); break;
        case MIR_RET: gen_ret(c); break;
    }
}

static void gen_function(GenContext *c, GenInput *input, int32_t f_index) {
    TirId value = input->global_deps.functions.ptr[f_index];
    int32_t mir_start = input->mir_result->ends[f_index];
    int32_t mir_end = input->mir_result->ends[f_index + 1];
    int32_t data_start = input->mir_result->data_starts[f_index];
    bool is_main = input->global_deps.main.id == value.id;

    c->tir.thread = &input->insts[f_index].deps;
    c->tmp_count = 0;
    c->alloc_count = 0;
    c->stack.len = 0;
    TirFunction t = tir_get_function(c->tir, value);
    TirId ret_type = tir_get_function_type(c->tir, t.type).ret;
    c->is_main = is_main;
    if (is_main) {
        fprintf(c->stream, "define i32 @main");
    } else {
        fprintf(c->stream, "define private ");
        gen_ret_type(c, ret_type);
        char const *name = tir_get_str(c->tir, t.name);
        fprintf(c->stream, " @%s", name);
    }
    gen_params(c, t.type);
    fprintf(c->stream, " {\n");
    c->blocks = 1;
    c->tmp_count++;
    c->alloc_count = c->tmp_count;
    c->data_top = data_start;
    for (int32_t i = mir_start; i < mir_end; i++) {
        switch ((MirTag) c->mir->insts.ptr[i]) {
            case MIR_ALLOC: gen_alloc(c); break;
            case MIR_ALLOC_VAR: gen_alloc_var(c); break;
            case MIR_CALL: gen_call_alloc(c); break;

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
            case MIR_INARROW:
            case MIR_SEXT:
            case MIR_ZEXT:
            case MIR_FTOI:
            case MIR_FTRUNC:
            case MIR_FEXT:
            case MIR_NOP:
            case MIR_ACCESS:
            case MIR_ADDRESS:
            case MIR_BR:
            case MIR_BR_IF:
            case MIR_BR_IF_NOT: {
                c->data_top += 1;
                break;
            }
            case MIR_INT: {
                c->data_top += 2;
                break;
            }
        }
    }
    c->data_top = data_start;
    c->has_overflow_block = false;
    for (int32_t i = mir_start; i < mir_end; i++) {
        if (i != mir_start && is_mir_terminator(c->mir->insts.ptr[i - 1])) {
            fprintf(c->stream, "L.%d:\n", c->blocks++);
        }
        gen_instruction(c, c->mir->insts.ptr[i]);
    }

    if (c->has_overflow_block) {
        fprintf(c->stream, "L.overflow:\n");
        fprintf(c->stream, "  call void @llvm.trap()\n");
        fprintf(c->stream, "  unreachable\n");
    }

    fprintf(c->stream, "}\n");
    assert(c->stack.len == 0);
    assert(
        f_index == input->global_deps.functions.len - 1
            ? (c->data_top == input->mir_result->mir.data.len)
            : (c->data_top == input->mir_result->data_starts[f_index + 1])
    );
}

static void gen_struct(GenContext *c, TirId type) {
    TirTaggedType t = tir_get_tagged_type(c->tir, type);
    TirStructType s = tir_get_struct_type(c->tir, t.inner);
    gen_struct_name(c, t.inner);
    fprintf(c->stream, " = type { ");

    for (int32_t i = 0; i < s.fields.len; i++) {
        if (i != 0) {
            fprintf(c->stream, ", ");
        }
        TirId field_type = s.fields.ptr[i];
        gen_type(c, field_type);
    }

    fprintf(c->stream, " }\n");
}

static void gen_thread(GenContext *c, int32_t thread, Tir *tir) {
    c->tir.thread = tir;
    c->thread = thread;

    for (int32_t i = 0; i < tir->structs.len; i++) {
        TirId type = tir->structs.ptr[i];
        gen_struct(c, type);
    }

    for (int32_t i = 0; i < tir->extern_vars.len; i++) {
        TirId value = tir->extern_vars.ptr[i];
        gen_extern_var(c, value);
    }

    for (int32_t i = 0; i < tir->extern_functions.len; i++) {
        TirId value = tir->extern_functions.ptr[i];
        gen_extern_function(c, value);
    }
}

void gen_llvm(GenInput *input, Target target) {
    FILE *stream = fopen("a.ll", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "%%slice = type { i%d, ptr }\n", (int) sizeof_pointer(target) * 8);

    GenContext c = {
        .target = target,
        .tir = {
            .global = &input->global_deps,
            .thread = NULL,
        },
        .mir = &input->mir_result->mir,
        .stream = stream,
    };

    gen_thread(&c, 0, &input->global_deps);
    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_thread(&c, i + 1, &input->insts[i].deps);
    }

    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_function(&c, input, i);
    }

    for (int32_t i = 0; i < c.strings.len; i++) {
        gen_string(&c, i, c.strings.ptr[i]);
    }

    fclose(stream);
}
