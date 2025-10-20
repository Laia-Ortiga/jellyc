#include "gen.h"

#include "gen-common.h"
#include "mir.h"
#include "type.h"
#include "fwd.h"
#include "util.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>

typedef struct {
    Mir *mir;
    Vec(MirOperand) stack;
    int32_t data_top;
    int32_t tmp_count;
    Target target;
    FILE *stream;
} GenContext;

static void gen_type_before(GenContext *c, MirTypeId type);
static void gen_type_after(GenContext *c, MirTypeId type);

static bool ptr_type_needs_parens(GenContext *c, MirTypeId type) {
    if (type.private_field_id == MIR_TYPE_FUNCTION) {
        return true;
    }
    if (type.private_field_id >= 0 && get_mir_type(c->mir, type).tag == MIR_TYPE_ARRAY) {
        return true;
    }
    return false;
}

static void gen_ptr_type_before(GenContext *c, MirTypeId type) {
    gen_type_before(c, type);

    if (ptr_type_needs_parens(c, type)) {
        fprintf(c->stream, "(*");
    } else {
        fprintf(c->stream, "*");
    }
}

static void gen_ptr_type_after(GenContext *c, MirTypeId type) {
    if (ptr_type_needs_parens(c, type)) {
        fprintf(c->stream, ")");
    }

    gen_type_after(c, type);
}

static void gen_function_signature(
    GenContext *c,
    char const *name,
    int32_t param_count,
    MirTypeId *params,
    MirTypeId ret
) {
    if (is_mir_type_aggregate(c->mir, ret)) {
        gen_ptr_type_before(c, ret);
    } else {
        gen_type_before(c, ret);
    }

    fprintf(c->stream, "%s(", name);
    bool c_has_params = param_count != 0;

    if (ret.private_field_id != MIR_TYPE_VOID && is_mir_type_aggregate(c->mir, ret)) {
        gen_ptr_type_before(c, ret);
        fprintf(c->stream, "ret");
        gen_ptr_type_after(c, ret);

        if (c_has_params) {
            fprintf(c->stream, ", ");
        }

        c_has_params = true;
    }

    if (c_has_params) {
        for (int32_t i = 0; i < param_count; i++) {
            if (i != 0) {
                fprintf(c->stream, ", ");
            }

            MirTypeId param_type = params[i];
            if (is_mir_type_aggregate(c->mir, param_type)) {
                gen_ptr_type_before(c, param_type);
                fprintf(c->stream, "v%d", i);
                gen_ptr_type_after(c, param_type);
            } else {
                gen_type_before(c, param_type);
                fprintf(c->stream, "v%d", i);
                gen_type_after(c, param_type);
            }
        }
    } else {
        fprintf(c->stream, "void");
    }

    fprintf(c->stream, ")");

    if (is_mir_type_aggregate(c->mir, ret)) {
        gen_ptr_type_after(c, ret);
    } else {
        gen_type_after(c, ret);
    }
}

static void gen_struct_name(GenContext *c, MirTypeId type) {
    fprintf(c->stream, "struct _S%d", type.private_field_id);
}

static void gen_type_before(GenContext *c, MirTypeId type) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8: fprintf(c->stream, "int8_t "); return;
        case MIR_TYPE_I16: fprintf(c->stream, "int16_t "); return;
        case MIR_TYPE_I32: fprintf(c->stream, "int32_t "); return;
        case MIR_TYPE_I64: fprintf(c->stream, "int64_t "); return;
        case MIR_TYPE_F32: fprintf(c->stream, "float "); return;
        case MIR_TYPE_F64: fprintf(c->stream, "double "); return;
        case MIR_TYPE_VOID: fprintf(c->stream, "void "); return;
        case MIR_TYPE_BOOL: fprintf(c->stream, "int8_t "); return;
        case MIR_TYPE_PTR: gen_ptr_type_before(c, (MirTypeId) {MIR_TYPE_VOID}); return;
        case MIR_TYPE_SLICE: fprintf(c->stream, "struct Slice "); return;
        default: {
            MirTypeUnion u = get_mir_type(c->mir, type);
            switch (u.tag) {
                case MIR_TYPE_ARRAY: {
                    gen_type_before(c, u.array.elem);
                    return;
                }
                case MIR_TYPE_FUNCTION: {
                    if (is_mir_type_aggregate(c->mir, u.function.ret)) {
                        gen_ptr_type_before(c, u.function.ret);
                    } else {
                        gen_type_before(c, u.function.ret);
                    }
                    fprintf(c->stream, "(*");
                    return;
                }
                case MIR_TYPE_STRUCT: {
                    gen_struct_name(c, type);
                    fprintf(c->stream, " ");
                    return;
                }
            }
            break;
        }
    }

    abort();
}

static void gen_type_after(GenContext *c, MirTypeId type) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8:
        case MIR_TYPE_I16:
        case MIR_TYPE_I32:
        case MIR_TYPE_I64:
        case MIR_TYPE_F32:
        case MIR_TYPE_F64:
        case MIR_TYPE_VOID:
        case MIR_TYPE_BOOL:
        case MIR_TYPE_SLICE: return;
        case MIR_TYPE_PTR: gen_ptr_type_after(c, (MirTypeId) {MIR_TYPE_VOID}); return;
        default: {
            MirTypeUnion u = get_mir_type(c->mir, type);
            switch (u.tag) {
                case MIR_TYPE_ARRAY: {
                    fprintf(c->stream, "[%ld]", u.array.length);
                    gen_type_after(c, u.array.elem);
                    return;
                }
                case MIR_TYPE_FUNCTION: {
                    fprintf(c->stream, ")(");
                    bool c_has_params = u.function.param_count != 0;

                    if (u.function.ret.private_field_id != MIR_TYPE_VOID && is_mir_type_aggregate(c->mir, u.function.ret)) {
                        gen_ptr_type_before(c, u.function.ret);
                        fprintf(c->stream, "ret");
                        gen_ptr_type_after(c, u.function.ret);

                        if (c_has_params) {
                            fprintf(c->stream, ", ");
                        }

                        c_has_params = true;
                    }

                    if (c_has_params) {
                        for (int32_t i = 0; i < u.function.param_count; i++) {
                            if (i != 0) {
                                fprintf(c->stream, ", ");
                            }

                            MirTypeId param_type = c->mir->type_extra.ptr[u.function.first_param + i];
                            if (is_mir_type_aggregate(c->mir, param_type)) {
                                gen_ptr_type_before(c, param_type);
                                fprintf(c->stream, "v%d", i);
                                gen_ptr_type_after(c, param_type);
                            } else {
                                gen_type_before(c, param_type);
                                fprintf(c->stream, "v%d", i);
                                gen_type_after(c, param_type);
                            }
                        }
                    } else {
                        fprintf(c->stream, "void");
                    }

                    fprintf(c->stream, ")");

                    if (is_mir_type_aggregate(c->mir, u.function.ret)) {
                        gen_ptr_type_after(c, u.function.ret);
                    } else {
                        gen_type_after(c, u.function.ret);
                    }
                    return;
                }
                case MIR_TYPE_STRUCT: {
                    return;
                }
            }
            break;
        }
    }

    abort();
}

static void gen_extern_var(GenContext *c, int32_t index) {
    MirGlobal *v = &c->mir->extern_vars.ptr[index];
    fprintf(c->stream, "extern ");
    gen_type_before(c, v->type);
    fprintf(c->stream, "%s", v->name);
    gen_type_after(c, v->type);
    fprintf(c->stream, ";\n");
}

static void gen_extern_function(GenContext *c, int32_t index) {
    MirGlobal *f = &c->mir->extern_functions.ptr[index];
    MirFunctionType type = get_mir_type(c->mir, f->type).function;

    gen_function_signature(
        c,
        f->name,
        type.param_count,
        c->mir->type_extra.ptr + type.first_param,
        type.ret
    );

    fprintf(c->stream, ";\n");
}

static void gen_function_decl(GenContext *c, int32_t index) {
    MirGlobal *f = &c->mir->functions.ptr[index];
    MirFunctionType type = get_mir_type(c->mir, f->type).function;
    fprintf(c->stream, "static ");

    gen_function_signature(
        c,
        f->name,
        type.param_count,
        c->mir->type_extra.ptr + type.first_param,
        type.ret
    );

    fprintf(c->stream, ";\n");
}

static void print_string(GenContext *c, char const *str) {
    fprintf(c->stream, "\"");
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] == '"') {
            fprintf(c->stream, "\\\"");
        } else if (str[i + 4] == '\\') {
            fprintf(c->stream, "\\\\");
        } else if (str[i + 4] >= 32 && str[i + 4] <= 126) {
            fprintf(c->stream, "%c", str[i + 4]);
        } else {
            fprintf(c->stream, "\\x%02X", (int) (unsigned char) str[i + 4]);
        }
    }
    fprintf(c->stream, "\"");
}

static MirOperand new_tmp(
    GenContext *c,
    bool is_lvalue,
    MirTypeId type
) {
    MirOperand operand = {
        .is_lvalue = is_lvalue,
        .tag = MIR_OPERAND_TMP,
        .type = type,
        .index = c->tmp_count++,
    };
    vec_push(&c->stack, operand);
    return operand;
}

static MirOperand introduce_temporary(
    GenContext *c,
    bool is_lvalue,
    MirTypeId type
) {
    fprintf(c->stream, "    ");

    if (is_lvalue) {
        gen_ptr_type_before(c, type);
    } else {
        gen_type_before(c, type);
    }

    MirOperand a = new_tmp(c, is_lvalue, type);
    fprintf(c->stream, "t%d", a.index);

    if (is_lvalue) {
        gen_ptr_type_after(c, type);
    } else {
        gen_type_after(c, type);
    }

    fprintf(c->stream, " = ");
    return a;
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

static void gen_operand(GenContext *c, MirOperand *a) {
    if (a->is_lvalue) {
        fprintf(c->stream, "(*");
    }
    switch (a->tag) {
        case MIR_OPERAND_INT: {
            fprintf(c->stream, "%ld", a->i);
            break;
        }
        case MIR_OPERAND_FLOAT: {
            fprintf(c->stream, "%f", a->f);
            break;
        }
        case MIR_OPERAND_NULL: {
            fprintf(c->stream, "0");
            break;
        }
        case MIR_OPERAND_STRING: {
            print_string(c, a->s);
            break;
        }
        case MIR_OPERAND_VARIABLE: {
            fprintf(c->stream, "v%d", a->index);
            break;
        }
        case MIR_OPERAND_GLOBAL: {
            fprintf(c->stream, "%s", a->s);
            break;
        }
        case MIR_OPERAND_TMP: {
            fprintf(c->stream, "t%d", a->index);
            break;
        }
    }
    if (a->is_lvalue) {
        fprintf(c->stream, ")");
    }
}

static MirOperand print_alloc(GenContext *c, MirTypeId type) {
    if (type.private_field_id == MIR_TYPE_VOID) {
        abort();
    }

    fprintf(c->stream, "    ");
    gen_type_before(c, type);
    MirOperand a = new_tmp(c, MIR_ALLOC, type);
    gen_operand(c, &a);
    gen_type_after(c, type);
    fprintf(c->stream, ";\n");
    return a;
}

static void gen_alloc(GenContext *c) {
    MirTypeId type = pop_type(c);
    print_alloc(c, type);
}

static void gen_alloc_var(GenContext *c) {
    MirTypeId type = pop_type(c);
    int32_t v = pop_data(c);
    fprintf(c->stream, "    ");
    gen_type_before(c, type);
    fprintf(c->stream, "v%d", v);
    gen_type_after(c, type);
    fprintf(c->stream, ";\n");
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_VARIABLE,
        .type = type,
        .index = v,
    };
    vec_push(&c->stack, operand);
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
        .is_lvalue = false,
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
        .is_lvalue = false,
        .tag = MIR_OPERAND_VARIABLE,
        .type = type,
        .index = index,
    };
    vec_push(&c->stack, operand);
}

static void gen_global(GenContext *c) {
    MirTypeId type = pop_type(c);
    int32_t *p = &c->mir->data.ptr[c->data_top];
    pop_data(c);
    pop_data(c);
    MirOperand operand = {
        .is_lvalue = false,
        .tag = MIR_OPERAND_GLOBAL,
        .type = type,
        .s = (char const *) (intptr_t) load_i64(p),
    };
    vec_push(&c->stack, operand);
}

static void gen_address(GenContext *c) {
    MirOperand a = pop_operand(c);
    introduce_temporary(c, false, (MirTypeId) {MIR_TYPE_PTR});
    fputs("&", c->stream);
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_deref(GenContext *c) {
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    introduce_temporary(c, true, type);
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_neg(GenContext *c) {
    MirOperand a = pop_operand(c);
    if (!is_mir_float_type(a.type)) {
        MirOperand result = new_tmp(c, false, a.type);
        fprintf(c->stream, "    ");
        gen_type_before(c, result.type);
        fprintf(c->stream, "t%d", result.index);
        gen_type_after(c, result.type);
        fprintf(c->stream, ";\n");

        fprintf(c->stream, "    if (__builtin_sub_overflow(0, ");
        gen_operand(c, &a);
        fprintf(c->stream, ", &");
        gen_operand(c, &result);
        fprintf(c->stream, ")) { __builtin_abort(); }\n");
    } else {
        introduce_temporary(c, false, a.type);
        fputs("-", c->stream);
        gen_operand(c, &a);
        fprintf(c->stream, ";\n");
    }
}

static void gen_not(GenContext *c) {
    MirOperand a = pop_operand(c);
    introduce_temporary(c, false, a.type);
    fputs("!", c->stream);
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_binary(GenContext *c, char const *op, char const *float_op) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    if (!is_mir_float_type(a.type)) {
        MirOperand result = new_tmp(c, false, a.type);
        fprintf(c->stream, "    ");
        gen_type_before(c, result.type);
        fprintf(c->stream, "t%d", result.index);
        gen_type_after(c, result.type);
        fprintf(c->stream, ";\n");

        fprintf(c->stream, "    if (%s(", op);
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, ", &");
        gen_operand(c, &result);
        fprintf(c->stream, ")) { __builtin_abort(); }\n");
    } else {
        introduce_temporary(c, false, a.type);
        gen_operand(c, &a);
        fprintf(c->stream, " %s ", float_op);
        gen_operand(c, &b);
        fprintf(c->stream, ";\n");
    }
}

static void gen_binary2(GenContext *c, char const *op) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    introduce_temporary(c, false, a.type);
    gen_operand(c, &a);
    fprintf(c->stream, " %s ", op);
    gen_operand(c, &b);
    fprintf(c->stream, ";\n");
}

static void gen_bool_binary(GenContext *c, char const *op) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    introduce_temporary(c, false, (MirTypeId) {MIR_TYPE_BOOL});
    gen_operand(c, &a);
    fprintf(c->stream, " %s ", op);
    gen_operand(c, &b);
    fprintf(c->stream, ";\n");
}

static void gen_div(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    if (!is_mir_float_type(a.type)) {
        fprintf(c->stream, "    if (");
        gen_operand(c, &b);
        fprintf(c->stream, " == 0) { __builtin_abort(); }\n");
        fprintf(c->stream, "    if (");
        gen_operand(c, &a);
        fprintf(c->stream, " < %" PRId64 " && ", -INT64_MAX);
        gen_operand(c, &b);
        fprintf(c->stream, " == -1) { __builtin_abort(); }\n");

        introduce_temporary(c, false, a.type);
        gen_operand(c, &a);
        fprintf(c->stream, " >= 0 ? ");
        gen_operand(c, &a);
        fprintf(c->stream, " / ");
        gen_operand(c, &b);
        fprintf(c->stream, " : ");
        gen_operand(c, &b);
        fprintf(c->stream, " > 0");
        fprintf(c->stream, " ? (");
        gen_operand(c, &a);
        fprintf(c->stream, " + 1) / ");
        gen_operand(c, &b);
        fprintf(c->stream, " - 1 : (");
        gen_operand(c, &a);
        fprintf(c->stream, " + 1) / ");
        gen_operand(c, &b);
        fprintf(c->stream, " + 1;\n");
    } else {
        introduce_temporary(c, false, a.type);
        gen_operand(c, &a);
        fprintf(c->stream, " / ");
        gen_operand(c, &b);
        fprintf(c->stream, ";\n");
    }
}

static void gen_mod(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);

    if (!is_mir_float_type(a.type)) {
        fprintf(c->stream, "    if (");
        gen_operand(c, &b);
        fprintf(c->stream, " == 0) { __builtin_abort(); }\n");

        int32_t abs_b = c->tmp_count++;
        fprintf(c->stream, "    int64_t t%d = ", abs_b);
        gen_operand(c, &b);
        fprintf(c->stream, " < 0 ? -");
        gen_operand(c, &b);
        fprintf(c->stream, " : ");
        gen_operand(c, &b);
        fprintf(c->stream, ";\n");

        int32_t rem = c->tmp_count++;
        fprintf(c->stream, "    int64_t t%d = ", rem);
        gen_operand(c, &a);
        fprintf(c->stream, " %% t%d;\n", abs_b);

        introduce_temporary(c, false, a.type);
        gen_operand(c, &a);
        fprintf(c->stream, " < 0 ? t%d + t%d : t%d;\n", rem, abs_b, rem);
    } else {
        introduce_temporary(c, false, a.type);
        fprintf(c->stream, "%s(", a.type.private_field_id == MIR_TYPE_F32 ? "__builtin_fmodf" : "__builtin_fmod");
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, ");\n");
    }
}

static void copy_c_value(GenContext *c, MirOperand *dst, MirOperand *src) {
    if (src->type.private_field_id >= 0
        && get_mir_type(c->mir, src->type).tag == MIR_TYPE_ARRAY
    ) {
        int64_t length = get_mir_type(c->mir, src->type).array.length;
        fprintf(c->stream, "    for (int64_t i = 0; i < %" PRId64 "; i++) {\n", length);
        fprintf(c->stream, "        ");
        if (dst) {
            gen_operand(c, dst);
        } else {
            fprintf(c->stream, "(*ret)");
        }
        fprintf(c->stream, "[i] = ");
        gen_operand(c, src);
        fprintf(c->stream, "[i];\n");
        fprintf(c->stream, "    }\n");
    } else {
        fprintf(c->stream, "    ");
        if (dst) {
            gen_operand(c, dst);
        } else {
            fprintf(c->stream, "*ret");
        }
        fprintf(c->stream, " = ");
        gen_operand(c, src);
        fprintf(c->stream, ";\n");
    }
}

static void gen_assign(GenContext *c) {
    MirOperand b = pop_operand(c);
    MirOperand a = pop_operand(c);
    copy_c_value(c, &a, &b);
}

static void gen_cast(GenContext *c) {
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    introduce_temporary(c, false, type);
    fprintf(c->stream, "(");
    gen_type_before(c, type);
    gen_type_after(c, type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_zext(GenContext *c) {
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    introduce_temporary(c, false, type);
    fprintf(c->stream, "(");
    gen_type_before(c, type);
    gen_type_after(c, type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);

    int64_t int_size;
    switch (type.private_field_id) {
        case MIR_TYPE_I8: {
            int_size = 1;
            break;
        }
        case MIR_TYPE_I16: {
            int_size = 2;
            break;
        }
        case MIR_TYPE_I32: {
            int_size = 4;
            break;
        }
        case MIR_TYPE_I64: {
            int_size = 8;
            break;
        }
        default: {
            abort();
        }
    }

    uint64_t mask = ((uint64_t) 1 << (int_size * 8)) - 1;
    fprintf(c->stream, " & 0x%lX;\n", mask);
}

static void gen_inarrow(GenContext *c) {
    MirOperand a = pop_operand(c);
    MirTypeId type = pop_type(c);
    int64_t min = 0;
    int64_t max = 0;

    switch (type.private_field_id) {
        case MIR_TYPE_I8: {
            min = INT8_MIN;
            max = INT8_MAX;
            break;
        }
        case MIR_TYPE_I16: {
            min = INT16_MIN;
            max = INT16_MAX;
            break;
        }
        case MIR_TYPE_I32: {
            min = INT32_MIN;
            max = INT32_MAX;
            break;
        }
        default: {
            abort();
        }
    }

    fprintf(c->stream, "    if (");
    gen_operand(c, &a);
    fprintf(c->stream, " < %" PRId64 " || ", min);
    gen_operand(c, &a);
    fprintf(c->stream, " > %" PRId64 ") { __builtin_abort(); }\n", max);

    introduce_temporary(c, false, type);
    fprintf(c->stream, "(");
    gen_type_before(c, type);
    gen_type_after(c, type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_call(GenContext *c) {
    MirTypeId ret_type = pop_type(c);
    int32_t arg_count = pop_data(c);
    bool implicit_return = is_mir_type_aggregate(c->mir, ret_type);
    MirOperand a;

    if (implicit_return) {
        a = print_alloc(c, ret_type);
        fprintf(c->stream, "    ");
    } else if (ret_type.private_field_id != MIR_TYPE_VOID) {
        a = introduce_temporary(c, false, ret_type);
    } else {
        fprintf(c->stream, "    ");
    }

    int32_t stack_elems = 1 + arg_count + (ret_type.private_field_id != MIR_TYPE_VOID);
    MirOperand *f = c->stack.ptr + c->stack.len - stack_elems;
    MirOperand *args = f + 1;

    gen_operand(c, f);
    fprintf(c->stream, "(");

    if (implicit_return) {
        fprintf(c->stream, "&");
        gen_operand(c, &a);

        if (arg_count) {
            fprintf(c->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(c->stream, ", ");
        }

        if (is_mir_type_aggregate(c->mir, args[i].type)) {
            fprintf(c->stream, "&");
        }
        gen_operand(c, &args[i]);
    }

    fprintf(c->stream, ");\n");
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
    introduce_temporary(c, true, elem_type);
    fprintf(c->stream, "&((");
    gen_ptr_type_before(c, elem_type);
    gen_ptr_type_after(c, elem_type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);
    fprintf(c->stream, ")[");
    gen_operand(c, &index);
    fprintf(c->stream, "];\n");
}

static void gen_slice_index(GenContext *c) {
    MirTypeId elem_type = pop_type(c);
    MirOperand index = pop_operand(c);
    MirOperand a = pop_operand(c);

    fprintf(c->stream, "    if (");
    gen_operand(c, &index);
    fprintf(c->stream, " < 0 || ");
    gen_operand(c, &index);
    fprintf(c->stream, " >= ");
    gen_operand(c, &a);
    fprintf(c->stream, "._0) { __builtin_abort(); }\n");

    introduce_temporary(c, true, elem_type);
    fprintf(c->stream, "&((");
    gen_ptr_type_before(c, elem_type);
    gen_ptr_type_after(c, elem_type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);
    fprintf(c->stream, "._1)[");
    gen_operand(c, &index);
    fprintf(c->stream, "];\n");
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
    introduce_temporary(c, true, field_type);
    fprintf(c->stream, "&((");
    gen_ptr_type_before(c, s.type);
    gen_ptr_type_after(c, s.type);
    fprintf(c->stream, ") &");
    gen_operand(c, &s);
    fprintf(c->stream, ")->_%d;\n", field);
}

static void gen_br(GenContext *c) {
    int32_t block = pop_data(c);
    fprintf(c->stream, "    goto L%d;\n", block);
}

static void gen_br_if(GenContext *c) {
    MirOperand condition = pop_operand(c);
    int32_t block = pop_data(c);
    fprintf(c->stream, "    if (");
    gen_operand(c, &condition);
    fprintf(c->stream, ") goto L%d;\n", block);
}

static void gen_br_if_not(GenContext *c) {
    MirOperand condition = pop_operand(c);
    int32_t block = pop_data(c);
    fprintf(c->stream, "    if (!");
    gen_operand(c, &condition);
    fprintf(c->stream, ") goto L%d;\n", block);
}

static void gen_ret_void(GenContext *c) {
    fprintf(c->stream, "    return;\n");
}

static void gen_ret(GenContext *c) {
    MirOperand a = pop_operand(c);

    if (!is_mir_type_aggregate(c->mir, a.type)) {
        fprintf(c->stream, "    return ");
        gen_operand(c, &a);
        fprintf(c->stream, ";\n");
    } else {
        copy_c_value(c, NULL, &a);
        fprintf(c->stream, "    return ret;\n");
    }
}

static void gen_instruction(GenContext *c, int32_t i) {
    MirTag tag = c->mir->insts.ptr[i];
    int32_t top = c->data_top;
    switch (tag) {
        case MIR_ALLOC: gen_alloc(c); break;
        case MIR_ALLOC_VAR: gen_alloc_var(c); break;
        case MIR_STACK_COPY: stack_copy(c); break;
        case MIR_STACK_COPY_AT: stack_copy_at(c); break;
        case MIR_STACK_POP: stack_pop(c); break;
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
        case MIR_GLOBAL_VAR: gen_global(c); break;
        case MIR_GLOBAL_FUNCTION: gen_global(c); break;
        case MIR_ASSIGN: gen_assign(c); break;
        case MIR_NEG: gen_neg(c); break;
        case MIR_NOT: gen_not(c); break;
        case MIR_ADDRESS: gen_address(c); break;
        case MIR_DEREF: gen_deref(c); break;
        case MIR_ADD: gen_binary(c, "__builtin_add_overflow", "+"); break;
        case MIR_SUB: gen_binary(c, "__builtin_sub_overflow", "-"); break;
        case MIR_MUL: gen_binary(c, "__builtin_mul_overflow", "*"); break;
        case MIR_DIV: gen_div(c); break;
        case MIR_MOD: gen_mod(c); break;
        case MIR_AND: gen_binary2(c, "&"); break;
        case MIR_OR: gen_binary2(c, "|"); break;
        case MIR_XOR: gen_binary2(c, "^"); break;
        case MIR_SHL: gen_binary2(c, "<<"); break;
        case MIR_SHR: gen_binary2(c, ">>"); break;
        case MIR_EQ: gen_bool_binary(c, "=="); break;
        case MIR_NE: gen_bool_binary(c, "!="); break;
        case MIR_LT: gen_bool_binary(c, "<"); break;
        case MIR_GT: gen_bool_binary(c, ">"); break;
        case MIR_LE: gen_bool_binary(c, "<="); break;
        case MIR_GE: gen_bool_binary(c, ">="); break;

        case MIR_ITOF:
        case MIR_ITRUNC:
        case MIR_SEXT:
        case MIR_FTOI:
        case MIR_FTRUNC:
        case MIR_FEXT: gen_cast(c); break;

        case MIR_ZEXT: gen_zext(c); break;
        case MIR_INARROW: gen_inarrow(c); break;
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
    switch (tag) {
        #define DATA(name, type) + (int32_t) (sizeof(type) / sizeof(int32_t))
        #define X(name, ...) case MIR_##name: { assert(c->data_top - top == (0 __VA_ARGS__)); break; }
        #include "mir-defs"
    }
}

static void gen_function(GenContext *c, GenInput *in, int32_t f_index) {
    MirGlobal *value = &c->mir->functions.ptr[f_index];
    int32_t mir_start = in->mir->ends[f_index];
    int32_t mir_end = in->mir->ends[f_index + 1];

    c->data_top = in->mir->data_starts[f_index];
    c->tmp_count = 0;
    c->stack.len = 0;

    fprintf(c->stream, "static ");
    MirFunctionType type = get_mir_type(c->mir, value->type).function;
    gen_function_signature(
        c,
        value->name,
        type.param_count,
        c->mir->type_extra.ptr + type.first_param,
        type.ret
    );

    fprintf(c->stream, " {\n");
    int blocks = 1;

    for (int32_t i = mir_start; i < mir_end; i++) {
        if (i != mir_start && is_mir_terminator(c->mir->insts.ptr[i - 1])) {
            fprintf(c->stream, "L%d:\n    ;\n", blocks++);
        }

        gen_instruction(c, i);
    }

    assert(c->stack.len == 0);
    assert(
        f_index == c->mir->functions.len - 1
            ? (c->data_top == in->mir->data.len)
            : (c->data_top == in->mir->data_starts[f_index + 1])
    );
    fprintf(c->stream, "}\n");
}

static void gen_struct_decl(GenContext *c, MirTypeId type) {
    MirTypeUnion u = get_mir_type(c->mir, type);
    if (u.tag == MIR_TYPE_STRUCT) {
        gen_struct_name(c, type);
        fprintf(c->stream, ";\n");
    }
}

static void gen_struct(GenContext *c, MirTypeId type) {
    MirTypeUnion u = get_mir_type(c->mir, type);
    if (u.tag == MIR_TYPE_STRUCT) {
        gen_struct_name(c, type);
        fprintf(c->stream, " {\n");
        int32_t first = u.struct_.first_field;
        int32_t count = u.struct_.field_count;

        for (int32_t i = 0; i < count; i++) {
            MirTypeId field_type = c->mir->type_extra.ptr[first + i];
            fprintf(c->stream, "    ");
            gen_type_before(c, field_type);
            fprintf(c->stream, "_%d", i);
            gen_type_after(c, field_type);
            fprintf(c->stream, ";\n");
        }

        fprintf(c->stream, "};\n");
    }
}

void gen_c(GenInput *in, Target target) {
    FILE *stream = fopen("a.c", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "#include <stdint.h>\n\n");
    int ptr_bits = sizeof_pointer(target) * 8;
    fprintf(stream, "struct Slice { int%d_t _0; void *_1; };\n\n", ptr_bits);

    GenContext c = {
        .target = target,
        .mir = in->mir,
        .stream = stream,
    };

    for (int32_t i = 0; i < in->mir->types.len; i++) {
        gen_struct_decl(&c, (MirTypeId) {i});
    }

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
        gen_function_decl(&c, i);
    }

    for (int32_t i = 0; i < in->mir->functions.len; i++) {
        gen_function(&c, in, i);
    }

    if (in->mir->main_function >= 0) {
        char const *name = in->mir->functions.ptr[in->mir->main_function].name;
        fprintf(c.stream, "int main(void) {\n");
        fprintf(c.stream, "    %s();", name);
        fprintf(c.stream, "    return 0;");
        fprintf(c.stream, "}\n");
    }

    fclose(stream);
}
