#include "gen.h"

#include "adt.h"
#include "gen-common.h"
#include "mir.h"
#include "type.h"
#include "fwd.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    Mir *mir;
    Vec(MirOperand) stack;
    int32_t data_top;
    int32_t tmp_count;
    int32_t alloc_count;
    Vec(char const *) strings;
    int32_t blocks;
    bool has_overflow_block;
    Target target;
    FILE *stream;
} GenContext;

static void gen_struct_name(GenContext *c, MirTypeId type) {
    fprintf(c->stream, "%%_S%d", type.private_field_id);
}

static void gen_type(GenContext *c, MirTypeId type) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8: fprintf(c->stream, "i8"); return;
        case MIR_TYPE_I16: fprintf(c->stream, "i16"); return;
        case MIR_TYPE_I32: fprintf(c->stream, "i32"); return;
        case MIR_TYPE_I64: fprintf(c->stream, "i64"); return;
        case MIR_TYPE_F32: fprintf(c->stream, "float"); return;
        case MIR_TYPE_F64: fprintf(c->stream, "double"); return;
        case MIR_TYPE_VOID: fprintf(c->stream, "void"); return;
        case MIR_TYPE_BOOL: fprintf(c->stream, "i1"); return;
        case MIR_TYPE_PTR: fprintf(c->stream, "ptr"); return;
        case MIR_TYPE_SLICE: fprintf(c->stream, "%%slice"); return;
        default: {
            MirTypeUnion u = get_mir_type(c->mir, type);
            switch (u.tag) {
                case MIR_TYPE_ARRAY: {
                    fprintf(c->stream, "[%" PRId64 " x ", u.array.length);
                    gen_type(c, u.array.elem);
                    fprintf(c->stream, "]");
                    return;
                }
                case MIR_TYPE_FUNCTION: {
                    fprintf(c->stream, "ptr");
                    return;
                }
                case MIR_TYPE_STRUCT: {
                    gen_struct_name(c, type);
                    return;
                }
            }
            break;
        }
    }

    abort();
}

static void gen_params(
    GenContext *c,
    int32_t param_count,
    MirTypeId *params,
    MirTypeId ret
) {
    fprintf(c->stream, "(");

    if (ret.private_field_id != MIR_TYPE_VOID && is_mir_type_aggregate(c->mir, ret)) {
        fprintf(c->stream, "ptr");
        c->tmp_count++;
        if (param_count != 0) {
            fprintf(c->stream, ", ");
        }
    }

    for (int32_t i = 0; i < param_count; i++) {
        if (i != 0) {
            fprintf(c->stream, ", ");
        }

        if (is_mir_type_aggregate(c->mir, params[i])) {
            fprintf(c->stream, "ptr");
        } else {
            gen_type(c, params[i]);
        }
        fprintf(c->stream, " %%v%d", i);
    }

    fprintf(c->stream, ")");
}

static void gen_ret_type(GenContext *c, MirTypeId type) {
    if (is_mir_type_aggregate(c->mir, type)) {
        fprintf(c->stream, "ptr");
        return;
    }
    gen_type(c, type);
}

static void gen_extern_var(GenContext *c, int32_t index) {
    MirGlobal *v = &c->mir->extern_vars.ptr[index];
    fprintf(c->stream, "@%s = external global ", v->name);
    gen_type(c, v->type);
    fprintf(c->stream, ", align %d\n", get_mir_type_alignment(c->mir, v->type, c->target));
}

static void gen_extern_function(GenContext *c, int32_t index) {
    MirGlobal *f = &c->mir->extern_functions.ptr[index];
    MirFunctionType type = get_mir_type(c->mir, f->type).function;
    fprintf(c->stream, "declare ");
    gen_ret_type(c, type.ret);
    fprintf(c->stream, " @%s", f->name);
    gen_params(
        c,
        type.param_count,
        c->mir->type_extra.ptr + type.first_param,
        type.ret
    );
    fprintf(c->stream, "\n");
}

static void print_string(GenContext *c, int32_t index, char const *str) {
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

static MirOperand pop_operand(GenContext *c) {
    assert(c->stack.len >= 1);
    return c->stack.ptr[--c->stack.len];
}

static int32_t pop_data(GenContext *c) {
    return c->mir->data.ptr[c->data_top++];
}

static MirTypeId pop_type(GenContext *c) {
    return (MirTypeId) {pop_data(c)};
}

static MirOperand new_tmp(GenContext *c, bool is_lvalue, MirTypeId type) {
    MirOperand operand = {
        .is_lvalue = is_lvalue,
        .tag = MIR_OPERAND_TMP,
        .type = type,
        .index = c->tmp_count++,
    };
    vec_push(&c->stack, operand);
    return operand;
}

static void gen_operand(GenContext *c, MirOperand *a) {
    switch (a->tag) {
        case MIR_OPERAND_INT: {
            fprintf(c->stream, "%ld", a->i);
            break;
        }
        case MIR_OPERAND_FLOAT: {
            union {
                double value;
                uint64_t bits;
            } f;
            f.value = a->f;
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
        case MIR_OPERAND_NULL: {
            fprintf(c->stream, "null");
            break;
        }
        case MIR_OPERAND_STRING: {
            fprintf(c->stream, "@s%d", c->strings.len);
            vec_push(&c->strings, a->s);
            break;
        }
        case MIR_OPERAND_VARIABLE: {
            fprintf(c->stream, "%%v%d", a->index);
            break;
        }
        case MIR_OPERAND_GLOBAL: {
            fprintf(c->stream, "@%s", a->s);
            break;
        }
        default: {
            fprintf(c->stream, "%%%d", a->index);
            break;
        }
    }
}

static MirOperand load_operand(GenContext *c, MirOperand *a) {
    if (a->is_lvalue) {
        int32_t tmp = c->tmp_count++;
        fprintf(c->stream, "  %%%d = load ", tmp);
        gen_type(c, a->type);
        fprintf(c->stream, ", ptr ");
        gen_operand(c, a);
        fprintf(c->stream, "\n");
        return (MirOperand) {
            .is_lvalue = false,
            .tag = MIR_OPERAND_TMP,
            .type = a->type,
            .index = tmp,
        };
    }
    return *a;
}

static void gen_alloc(GenContext *c) {
    MirTypeId local_type = pop_type(c);
    assert(local_type.private_field_id != MIR_TYPE_VOID);
    fprintf(c->stream, "  %%%d = alloca ", c->tmp_count++);
    gen_type(c, local_type);
    fprintf(c->stream, "\n");
}

static void gen_alloc_var(GenContext *c) {
    MirTypeId local_type = pop_type(c);
    int32_t v = pop_data(c);
    assert(local_type.private_field_id != MIR_TYPE_VOID);
    fprintf(c->stream, "  %%v%d = alloca ", v);
    gen_type(c, local_type);
    fprintf(c->stream, "\n");
}

static void gen_call_alloc(GenContext *c) {
    MirTypeId ret_type = pop_type(c);
    pop_data(c);
    if (is_mir_type_aggregate(c->mir, ret_type)) {
        fprintf(c->stream, "  %%%d = alloca ", c->tmp_count++);
        gen_type(c, ret_type);
        fprintf(c->stream, "\n");
    }
}

static void gen_alloc2(GenContext *c) {
    MirTypeId local_type = pop_type(c);
    MirOperand operand = {
        .is_lvalue = true,
        .tag = MIR_OPERAND_TMP,
        .type = local_type,
        .index = c->alloc_count++,
    };
    vec_push(&c->stack, operand);
}

static void gen_alloc_var2(GenContext *c) {
    MirTypeId local_type = pop_type(c);
    int32_t v = pop_data(c);
    MirOperand operand = {
        .is_lvalue = true,
        .tag = MIR_OPERAND_VARIABLE,
        .type = local_type,
        .index = v,
    };
    vec_push(&c->stack, operand);
}

static void gen_overflow_check(GenContext *c, int32_t condition) {
    int32_t next_block = c->tmp_count++;
    fprintf(
        c->stream,
        "  br i1 %%%d, label %%L.overflow, label %%%d\n",
        condition,
        next_block
    );
    c->has_overflow_block = true;
}

static void gen_neg(GenContext *c) {
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    if (is_mir_float_type(a.type)) {
        fprintf(c->stream, "  %%%d = fsub ", new_tmp(c, false, a.type).index);
        gen_type(c, a.type);
        fprintf(c->stream, " 0.0, ");
        gen_operand(c, &a);
        fprintf(c->stream, "\n");
        return;
    }
    int32_t s = c->tmp_count++;
    fprintf(c->stream, "  %%%d = call {", s);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} @llvm.ssub.with.overflow.");
    gen_type(c, a.type);
    fprintf(c->stream, "(");
    gen_type(c, a.type);
    fprintf(c->stream, " 0, ");
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ")\n");

    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = extractvalue {", overflowed);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} %%%d, 1\n", s);

    gen_overflow_check(c, overflowed);

    fprintf(c->stream, "  %%%d = extractvalue {", new_tmp(c, false, a.type).index);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} %%%d, 0\n", s);
}

static void gen_not(GenContext *c) {
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    fprintf(c->stream, "  %%%d = xor ", new_tmp(c, false, a.type).index);
    gen_type(c, a.type);
    fprintf(c->stream, " -1, ");
    gen_operand(c, &a);
    fprintf(c->stream, "\n");
}

static void gen_binary(GenContext *c, char const *op) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
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
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (is_mir_float_type(a.type)) {
        fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, a.type).index, float_op);
        gen_type(c, a.type);
        fprintf(c->stream, " ");
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, "\n");
        return;
    }
    int32_t s = c->tmp_count++;
    fprintf(c->stream, "  %%%d = call {", s);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} @llvm.s%s.with.overflow.", op);
    gen_type(c, a.type);
    fprintf(c->stream, "(");
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &b);
    fprintf(c->stream, ")\n");

    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = extractvalue {", overflowed);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} %%%d, 1\n", s);

    gen_overflow_check(c, overflowed);

    fprintf(c->stream, "  %%%d = extractvalue {", new_tmp(c, false, a.type).index);
    gen_type(c, a.type);
    fprintf(c->stream, ", i1} %%%d, 0\n", s);
}

static void gen_div(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (is_mir_float_type(a.type)) {
        fprintf(c->stream, "  %%%d = fdiv ", new_tmp(c, false, a.type).index);
        gen_type(c, a.type);
        fprintf(c->stream, " ");
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, "\n");
        return;
    }

    int32_t div_by_zero = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp eq i64 ", div_by_zero);
    gen_operand(c, &b);
    fprintf(c->stream, ", 0\n");

    assert(a.type.private_field_id == MIR_TYPE_I64);
    int64_t min = INT64_MIN;
    int32_t o1 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp eq i64 ", o1);
    gen_operand(c, &a);
    fprintf(c->stream, ", %" PRId64 "\n", min);

    int32_t o2 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp eq i64 ", o2);
    gen_operand(c, &b);
    fprintf(c->stream, ", -1\n");

    int32_t o3 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = and i1 %%%d, %%%d\n", o3, o1, o2);
    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = or i1 %%%d, %%%d\n", overflowed, div_by_zero, o3);

    gen_overflow_check(c, overflowed);

    int32_t a_neg = c->tmp_count++;
    fprintf(c->stream, "  %%%d = ashr i64 ", a_neg);
    gen_operand(c, &a);
    fprintf(c->stream, ", 63\n");

    int32_t a2 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = sub i64 ", a2);
    gen_operand(c, &a);
    fprintf(c->stream, ", %%%d\n", a_neg);

    int32_t q = c->tmp_count++;
    fprintf(c->stream, "  %%%d = sdiv ", q);
    gen_type(c, a.type);
    fprintf(c->stream, " %%%d, ", a2);
    gen_operand(c, &b);
    fprintf(c->stream, "\n");

    int32_t b_neg = c->tmp_count++;
    fprintf(c->stream, "  %%%d = ashr i64 ", b_neg);
    gen_operand(c, &b);
    fprintf(c->stream, ", 63\n");

    int32_t b_neg2 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = or i64 %%%d, 1\n", b_neg2, b_neg);

    int32_t b_neg3 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = and i64 %%%d, %%%d\n", b_neg3, b_neg2, a_neg);

    fprintf(
        c->stream,
        "  %%%d = sub i64 %%%d, %%%d\n",
        new_tmp(c, false, a.type).index,
        q,
        b_neg3
    );
}

static void gen_rem(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (is_mir_float_type(a.type)) {
        fprintf(c->stream, "  %%%d = frem ", new_tmp(c, false, a.type).index);
        gen_type(c, a.type);
        fprintf(c->stream, " ");
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, "\n");
        return;
    }

    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp eq i64 ", overflowed);
    gen_operand(c, &b);
    fprintf(c->stream, ", 0\n");

    gen_overflow_check(c, overflowed);
    int32_t prev_block = c->tmp_count - 1;

    int32_t abs_b = c->tmp_count++;
    fprintf(c->stream, "  %%%d = call i64 @llvm.abs.i64(i64 ", abs_b);
    gen_operand(c, &b);
    fprintf(c->stream, ", i1 0)\n");

    int32_t rem = c->tmp_count++;
    fprintf(c->stream, "  %%%d = srem ", rem);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", %%%d\n", abs_b);

    int32_t is_neg = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp slt i64 ", is_neg);
    gen_operand(c, &a);
    fprintf(c->stream, ", 0\n");

    int32_t negative_block = c->tmp_count++;
    int32_t neg_path = c->tmp_count++;
    int32_t next_block = c->tmp_count++;
    fprintf(
        c->stream,
        "  br i1 %%%d, label %%%d, label %%%d\n",
        is_neg,
        negative_block,
        next_block
    );

    fprintf(c->stream, "  %%%d = add i64 ", neg_path);
    gen_operand(c, &a);
    fprintf(c->stream, ", %%%d\n", abs_b);
    fprintf(c->stream, "  br label %%%d\n", next_block);

    fprintf(c->stream, "%d:\n", next_block);
    fprintf(
        c->stream,
        "  %%%d = phi i64 [ %%%d, %%%d ], [ %%%d, %%%d ]\n",
        new_tmp(c, false, a.type).index,
        rem,
        prev_block,
        neg_path,
        negative_block
    );
}

static void gen_overloaded_cmp(
    GenContext *c,
    char const *op,
    char const *float_op
) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);
    b = load_operand(c, &b);
    if (is_mir_float_type(a.type)) {
        op = float_op;
    }
    fprintf(c->stream, "  %%%d = %s ", new_tmp(c, false, (MirTypeId) {MIR_TYPE_BOOL}).index, op);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", ");
    gen_operand(c, &b);
    fprintf(c->stream, "\n");
}

static void stack_copy(GenContext *c) {
    assert(c->stack.len >= 1);
    MirOperand a = c->stack.ptr[c->stack.len - 1];
    vec_push(&c->stack, a);
}

static void stack_copy_at(GenContext *c) {
    int32_t index = pop_data(c);
    assert(c->stack.len + index >= 0);
    MirOperand a = c->stack.ptr[c->stack.len + index];
    vec_push(&c->stack, a);
}

static void stack_pop(GenContext *c) {
    assert(c->stack.len >= 1);
    c->stack.len -= 1;
}

static void gen_bool(GenContext *c, bool value) {
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_INT,
        .type = {MIR_TYPE_BOOL},
        .i = value,
    };
    vec_push(&c->stack, operand);
}

static void gen_int1(GenContext *c, MirType type) {
    int32_t a = pop_data(c);
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_INT,
        .type = {type},
        .i = a,
    };
    vec_push(&c->stack, operand);
}

static void gen_int2(GenContext *c, MirType type) {
    int32_t *p = &c->mir->data.ptr[c->data_top];
    pop_data(c);
    pop_data(c);
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_INT,
        .type = {type},
        .i = load_i64(p),
    };
    vec_push(&c->stack, operand);
}

static void gen_f32(GenContext *c) {
    int32_t a = pop_data(c);
    float f;
    memcpy(&f, &a, sizeof(a));
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_FLOAT,
        .type = {MIR_TYPE_F32},
        .f = f,
    };
    vec_push(&c->stack, operand);
}

static void gen_f64(GenContext *c) {
    int32_t *p = &c->mir->data.ptr[c->data_top];
    pop_data(c);
    pop_data(c);
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_FLOAT,
        .type = {MIR_TYPE_F64},
        .f = load_f64(p),
    };
    vec_push(&c->stack, operand);
}

static void gen_null(GenContext *c) {
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_NULL,
        .type = {MIR_TYPE_PTR},
    };
    vec_push(&c->stack, operand);
}

static void gen_string(GenContext *c) {
    MirTypeId type = pop_type(c);
    int32_t *p = &c->mir->data.ptr[c->data_top];
    pop_data(c);
    pop_data(c);
    MirOperand operand = {
        .is_lvalue = true,
        .tag = MIR_OPERAND_STRING,
        .type = type,
        .s = (char const *) (intptr_t) load_i64(p),
    };
    vec_push(&c->stack, operand);
}

static void gen_parameter(GenContext *c) {
    MirTypeId type = pop_type(c);
    int32_t index = pop_data(c);
    MirOperand operand = {
        .is_lvalue = is_mir_type_aggregate(c->mir, type),
        .tag = MIR_OPERAND_VARIABLE,
        .type = type,
        .index = index,
    };
    vec_push(&c->stack, operand);
}

static void gen_variable(GenContext *c) {
    MirTypeId type = pop_type(c);
    int32_t index = pop_data(c);
    MirOperand operand = {
        .is_lvalue = true,
        .tag = MIR_OPERAND_VARIABLE,
        .type = type,
        .index = index,
    };
    vec_push(&c->stack, operand);
}

static void gen_global(GenContext *c, bool is_lvalue) {
    MirTypeId type = pop_type(c);
    int32_t *p = &c->mir->data.ptr[c->data_top];
    pop_data(c);
    pop_data(c);
    MirOperand operand = {
        .is_lvalue = is_lvalue,
        .tag = MIR_OPERAND_GLOBAL,
        .type = type,
        .s = (char const *) (intptr_t) load_i64(p),
    };
    vec_push(&c->stack, operand);
}

static void gen_address(GenContext *c) {
    MirOperand a = pop_operand(c);
    assert(a.is_lvalue);
    a.is_lvalue = false;
    a.type = (MirTypeId) {MIR_TYPE_PTR};
    vec_push(&c->stack, a);
}

static void gen_deref(GenContext *c) {
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    a = load_operand(c, &a);
    a.is_lvalue = true;
    a.type = type;
    vec_push(&c->stack, a);
}

static void gen_assign(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
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
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
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
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    a = load_operand(c, &a);
    MirOperand result = new_tmp(c, false, type);
    fprintf(c->stream, "  %%%d = trunc ", result.index);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, " to ");
    gen_type(c, type);
    fprintf(c->stream, "\n");

    int32_t b = c->tmp_count++;
    fprintf(c->stream, "  %%%d = sext ", b);
    gen_type(c, result.type);
    fprintf(c->stream, " ");
    gen_operand(c, &result);
    fprintf(c->stream, " to ");
    gen_type(c, a.type);
    fprintf(c->stream, "\n");

    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp ne ", overflowed);
    gen_type(c, a.type);
    fprintf(c->stream, " ");
    gen_operand(c, &a);
    fprintf(c->stream, ", %%%d\n", b);

    gen_overflow_check(c, overflowed);
}

static void gen_call(GenContext *c) {
    MirTypeId ret_type = pop_type(c);
    int32_t arg_count = pop_data(c);
    bool implicit_return = is_mir_type_aggregate(c->mir, ret_type);

    int32_t stack_elems = 1 + arg_count;
    int32_t f_index = c->stack.len - stack_elems;
    MirOperand f = load_operand(c, &c->stack.ptr[f_index]);

    for (int32_t i = 0; i < arg_count; i++) {
        if (!is_mir_type_aggregate(c->mir, c->stack.ptr[f_index + 1 + i].type)) {
            c->stack.ptr[f_index + 1 + i] = load_operand(c, &c->stack.ptr[f_index + 1 + i]);
        }
    }

    MirOperand a;
    fprintf(c->stream, "  ");
    if (ret_type.private_field_id != MIR_TYPE_VOID) {
        a = new_tmp(c, implicit_return, ret_type);
        fprintf(c->stream, "%%%d = ", a.index);
        stack_elems++;
        if (implicit_return) {
            a = (MirOperand) {
                .is_lvalue = true,
                .tag = MIR_OPERAND_TMP,
                .type = a.type,
                .index =  c->alloc_count++,
            };
        }
    }

    fprintf(c->stream, "call ");
    if (implicit_return) {
        fprintf(c->stream, "ptr");
    } else {
        gen_type(c, ret_type);
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

        if (is_mir_type_aggregate(c->mir, c->stack.ptr[f_index + 1 + i].type)) {
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

    if (ret_type.private_field_id != MIR_TYPE_VOID) {
        vec_push(&c->stack, a);
    }
}

static void gen_index(GenContext *c) {
    MirTypeId elem_type = pop_type(c);
    MirOperand index = pop_operand(c);
    MirOperand a = pop_operand(c);
    index = load_operand(c, &index);
    int32_t tmp = new_tmp(c, true, elem_type).index;
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
    MirTypeId elem_type = pop_type(c);
    MirOperand index = pop_operand(c);
    MirOperand a = pop_operand(c);
    index = load_operand(c, &index);
    assert(a.is_lvalue);

    int32_t length_ptr = c->tmp_count++;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", length_ptr);
    gen_type(c, a.type);
    fprintf(c->stream, ", ptr ");
    gen_operand(c, &a);
    fprintf(c->stream, ", i64 0, i32 0\n");

    int32_t length = c->tmp_count++;
    fprintf(c->stream, "  %%%d = load ", length);
    gen_type(c, index.type);
    fprintf(c->stream, ", ptr %%%d\n", length_ptr);

    int32_t overflowed = c->tmp_count++;
    fprintf(c->stream, "  %%%d = icmp uge ", overflowed);
    gen_type(c, index.type);
    fprintf(c->stream, " ");
    gen_operand(c, &index);
    fprintf(c->stream, ", %%%d\n", length);

    gen_overflow_check(c, overflowed);

    int32_t tmp1 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp1);
    gen_type(c, a.type);
    fprintf(c->stream, ", ptr ");
    gen_operand(c, &a);
    fprintf(c->stream, ", i64 0, i32 1\n");

    int32_t tmp2 = c->tmp_count++;
    fprintf(c->stream, "  %%%d = load ptr, ptr %%%d\n", tmp2, tmp1);

    int32_t tmp3 = new_tmp(c, true, elem_type).index;
    fprintf(c->stream, "  %%%d = getelementptr inbounds ", tmp3);
    gen_type(c, elem_type);
    fprintf(c->stream, ", ptr %%%d, i64 ", tmp2);
    gen_operand(c, &index);
    fprintf(c->stream, "\n");
}

static void gen_access(GenContext *c) {
    MirOperand s = pop_operand(c);
    int32_t field = pop_data(c);
    MirTypeId field_type;
    if (s.type.private_field_id == MIR_TYPE_SLICE) {
        switch (field) {
            case 0: field_type = (MirTypeId) {c->target == TARGET_ISIZE_64 ? MIR_TYPE_I64 : MIR_TYPE_I32}; break;
            case 1: field_type = (MirTypeId) {MIR_TYPE_PTR}; break;
            default: abort();
        }
    } else {
        field_type = c->mir->type_extra.ptr[get_mir_type(c->mir, s.type).struct_.first_field + field];
    }
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
    MirOperand condition = pop_operand(c);
    int32_t block = pop_data(c);
    condition = load_operand(c, &condition);
    fprintf(c->stream, "  br i1 ");
    gen_operand(c, &condition);
    fprintf(c->stream, ", label %%L.%d, label %%L.%d\n", block, c->blocks);
}

static void gen_br_if_not(GenContext *c) {
    MirOperand condition = pop_operand(c);
    int32_t block = pop_data(c);
    condition = load_operand(c, &condition);
    fprintf(c->stream, "  br i1 ");
    gen_operand(c, &condition);
    fprintf(c->stream, ", label %%L.%d, label %%L.%d\n", c->blocks, block);
}

static void gen_ret_void(GenContext *c) {
    fprintf(c->stream, "  ret void\n");
}

static void gen_ret(GenContext *c) {
    MirOperand a = pop_operand(c);
    a = load_operand(c, &a);

    if (!is_mir_type_aggregate(c->mir, a.type)) {
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
        case MIR_FALSE: gen_bool(c, false); break;
        case MIR_TRUE: gen_bool(c, true); break;
        case MIR_I8: gen_int1(c, MIR_TYPE_I8); break;
        case MIR_I16: gen_int1(c, MIR_TYPE_I16); break;
        case MIR_I32: gen_int1(c, MIR_TYPE_I32); break;
        case MIR_I64: gen_int2(c, MIR_TYPE_I64); break;
        case MIR_F32: gen_f32(c); break;
        case MIR_F64: gen_f64(c); break;
        case MIR_NULL: gen_null(c); break;
        case MIR_STRING: gen_string(c); break;
        case MIR_PARAMETER: gen_parameter(c); break;
        case MIR_VARIABLE: gen_variable(c); break;
        case MIR_GLOBAL_VAR: gen_global(c, true); break;
        case MIR_GLOBAL_FUNCTION: gen_global(c, false); break;
        case MIR_ADDRESS: gen_address(c); break;
        case MIR_DEREF: gen_deref(c); break;
        case MIR_ASSIGN: gen_assign(c); break;
        case MIR_NEG: gen_neg(c); break;
        case MIR_NOT: gen_not(c); break;
        case MIR_ADD: gen_overloaded_binary(c, "add", "fadd"); break;
        case MIR_SUB: gen_overloaded_binary(c, "sub", "fsub"); break;
        case MIR_MUL: gen_overloaded_binary(c, "mul", "fmul"); break;
        case MIR_DIV: gen_div(c); break;
        case MIR_MOD: gen_rem(c); break;
        case MIR_AND: gen_binary(c, "and"); break;
        case MIR_OR: gen_binary(c, "or"); break;
        case MIR_XOR: gen_binary(c, "xor"); break;
        case MIR_SHL: gen_binary(c, "shl"); break;
        case MIR_SHR: gen_binary(c, "ashr"); break;
        case MIR_EQ: gen_overloaded_cmp(c, "icmp eq", "fcmp ueq"); break;
        case MIR_NE: gen_overloaded_cmp(c, "icmp ne", "fcmp une"); break;
        case MIR_LT: gen_overloaded_cmp(c, "icmp slt", "fcmp ult"); break;
        case MIR_GT: gen_overloaded_cmp(c, "icmp sgt", "fcmp ugt"); break;
        case MIR_LE: gen_overloaded_cmp(c, "icmp sle", "fcmp ule"); break;
        case MIR_GE: gen_overloaded_cmp(c, "icmp sge", "fcmp uge"); break;

        case MIR_ITOF: gen_cast(c, "sitofp"); break;
        case MIR_ITRUNC: gen_cast(c, "trunc"); break;
        case MIR_INARROW: gen_narrow(c); break;
        case MIR_SEXT: gen_cast(c, "sext"); break;
        case MIR_ZEXT: gen_cast(c, "zext"); break;
        case MIR_FTOI: gen_cast(c, "fptosi"); break;
        case MIR_FTRUNC: gen_cast(c, "fptrunc"); break;
        case MIR_FEXT: gen_cast(c, "fpext"); break;

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
    MirGlobal *value = &c->mir->functions.ptr[f_index];
    int32_t mir_start = input->mir->ends[f_index];
    int32_t mir_end = input->mir->ends[f_index + 1];
    int32_t data_start = input->mir->data_starts[f_index];

    c->tmp_count = 0;
    c->alloc_count = 0;
    c->stack.len = 0;
    fprintf(c->stream, "define private ");
    MirFunctionType type = get_mir_type(c->mir, value->type).function;
    gen_ret_type(c, type.ret);
    fprintf(c->stream, " @%s", value->name);
    gen_params(
        c,
        type.param_count,
        c->mir->type_extra.ptr + type.first_param,
        type.ret
    );
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
            case MIR_FALSE:
            case MIR_TRUE:
            case MIR_NEG:
            case MIR_NOT:
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
            case MIR_RET_VOID:
            case MIR_RET:
            case MIR_NULL:
            case MIR_ADDRESS: {
                break;
            }
            case MIR_DEREF:
            case MIR_INDEX:
            case MIR_SLICE_INDEX:
            case MIR_STACK_COPY_AT:
            case MIR_ITOF:
            case MIR_ITRUNC:
            case MIR_INARROW:
            case MIR_SEXT:
            case MIR_ZEXT:
            case MIR_FTOI:
            case MIR_FTRUNC:
            case MIR_FEXT:
            case MIR_ACCESS:
            case MIR_BR:
            case MIR_BR_IF:
            case MIR_BR_IF_NOT:
            case MIR_I8:
            case MIR_I16:
            case MIR_I32:
            case MIR_F32: {
                c->data_top += 1;
                break;
            }
            case MIR_I64:
            case MIR_F64:
            case MIR_PARAMETER:
            case MIR_VARIABLE: {
                c->data_top += 2;
                break;
            }
            case MIR_STRING:
            case MIR_GLOBAL_VAR:
            case MIR_GLOBAL_FUNCTION: {
                c->data_top += 3;
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
        f_index == input->mir->functions.len - 1
            ? (c->data_top == input->mir->data.len)
            : (c->data_top == input->mir->data_starts[f_index + 1])
    );
}

static void gen_struct(GenContext *c, MirTypeId type) {
    MirTypeUnion u = get_mir_type(c->mir, type);
    if (u.tag == MIR_TYPE_STRUCT) {
        gen_struct_name(c, type);
        fprintf(c->stream, " = type { ");
        int32_t first = u.struct_.first_field;
        int32_t count = u.struct_.field_count;

        for (int32_t i = 0; i < count; i++) {
            if (i != 0) {
                fprintf(c->stream, ", ");
            }
            MirTypeId field_type = c->mir->type_extra.ptr[first + i];
            gen_type(c, field_type);
        }

        fprintf(c->stream, " }\n");
    }
}

void gen_llvm(GenInput *in, Target target) {
    FILE *stream = fopen("a.ll", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "%%slice = type { i%d, ptr }\n", (int) sizeof_pointer(target) * 8);

    GenContext c = {
        .target = target,
        .mir = in->mir,
        .stream = stream,
    };

    for (int32_t i = 0; i < in->mir->types.len; i++) {
        gen_struct(&c, (MirTypeId) {i});
    }

    for (int32_t i = 0; i < in->mir->extern_vars.len; i++) {
        gen_extern_var(&c, i);
    }

    for (int32_t i = 0; i < in->mir->extern_functions.len; i++) {
        gen_extern_function(&c, i);
    }

    for (int32_t i = 0; i < in->mir->functions.len; i++) {
        gen_function(&c, in, i);
    }

    if (in->mir->main_function >= 0) {
        char const *name = in->mir->functions.ptr[in->mir->main_function].name;
        fprintf(c.stream, "define i32 @main() {\n");
        fprintf(c.stream, "  call void @%s()\n", name);
        fprintf(c.stream, "  ret i32 0\n");
        fprintf(c.stream, "}\n");
    }

    for (int32_t i = 0; i < c.strings.len; i++) {
        print_string(&c, i, c.strings.ptr[i]);
    }

    fclose(stream);
}
