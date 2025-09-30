#include "gen.h"

#include "mir.h"
#include "tir.h"
#include "fwd.h"
#include "util.h"

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
    TirId return_type;
    bool is_main;
    Target target;
    TirContext tir;
    FILE *stream;
} GenContext;

static void gen_type_before(GenContext *ctx, TirId type);
static void gen_type_after(GenContext *ctx, TirId type);

static bool ptr_type_needs_parens(GenContext *ctx, TirId type) {
    return get_term_tag(ctx->tir, type) == TIR_ARRAY_TYPE || get_term_tag(ctx->tir, type) == TIR_FUNCTION_TYPE;
}

static void gen_ptr_type_before(GenContext *ctx, TirId type) {
    gen_type_before(ctx, type);

    if (ptr_type_needs_parens(ctx, type)) {
        fprintf(ctx->stream, "(*");
    } else {
        fprintf(ctx->stream, "*");
    }
}

static void gen_ptr_type_after(GenContext *ctx, TirId type) {
    if (ptr_type_needs_parens(ctx, type)) {
        fprintf(ctx->stream, ")");
    }

    gen_type_after(ctx, type);
}

static bool is_type_passed_by_ptr(GenContext *ctx, TirId type) {
    if (is_aggregate_type(ctx->tir, type)) {
        return true;
    }

    return false;
}

static void gen_params(GenContext *ctx, TirId type) {
    fprintf(ctx->stream, "(");
    FunctionType func_type = get_function_type(ctx->tir, type);
    bool c_has_params = func_type.param_count != 0;

    if (func_type.ret.id != TYPE_VOID && is_type_passed_by_ptr(ctx, func_type.ret)) {
        gen_ptr_type_before(ctx, func_type.ret);
        fprintf(ctx->stream, "ret");
        gen_ptr_type_after(ctx, func_type.ret);

        if (c_has_params) {
            fprintf(ctx->stream, ", ");
        }

        c_has_params = true;
    }

    if (c_has_params) {
        for (int32_t i = 0; i < func_type.param_count; i++) {
            if (i != 0) {
                fprintf(ctx->stream, ", ");
            }

            TirId param_type = get_function_type_param(ctx->tir, type, i);
            if (is_type_passed_by_ptr(ctx, param_type)) {
                gen_ptr_type_before(ctx, param_type);
                fprintf(ctx->stream, "v%d", i);
                gen_ptr_type_after(ctx, param_type);
            } else {
                gen_type_before(ctx, param_type);
                fprintf(ctx->stream, "v%d", i);
                gen_type_after(ctx, param_type);
            }
        }
    } else {
        fprintf(ctx->stream, "void");
    }

    fprintf(ctx->stream, ")");
}

static void gen_type_before(GenContext *ctx, TirId type) {
    switch (get_term_tag(ctx->tir, type)) {
        case TIR_PRIMITIVE_TYPE: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_VOID: fprintf(ctx->stream, "void "); return;

                case TYPE_i8: fprintf(ctx->stream, "int8_t "); return;
                case TYPE_i16: fprintf(ctx->stream, "int16_t "); return;
                case TYPE_i32: fprintf(ctx->stream, "int32_t "); return;
                case TYPE_i64: fprintf(ctx->stream, "int64_t "); return;

                case TYPE_isize: fprintf(ctx->stream, "int%d_t ", sizeof_pointer(ctx->target) * 8); return;

                case TYPE_f32: fprintf(ctx->stream, "float "); return;
                case TYPE_f64: fprintf(ctx->stream, "double "); return;

                case TYPE_byte: fprintf(ctx->stream, "char "); return;

                case TYPE_bool: fprintf(ctx->stream, "unsigned char "); return;

                default: break;
            }
            break;
        }
        case TIR_ARRAY_TYPE:{
            gen_type_before(ctx, get_array_type(ctx->tir, type).elem);
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            fprintf(ctx->stream, "int%d_t ", sizeof_pointer(ctx->target) * 8);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            gen_ptr_type_before(ctx, ptype(VOID));
            return;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            fprintf(ctx->stream, "struct Slice ");
            return;
        }
        case TIR_FUNCTION_TYPE: {
            TirId ret = get_function_type(ctx->tir, type).ret;

            if (ret.id != TYPE_VOID) {
                if (is_type_passed_by_ptr(ctx, ret)) {
                    gen_ptr_type_before(ctx, ret);
                } else {
                    gen_type_before(ctx, ret);
                }
            } else {
                fprintf(ctx->stream, "void ");
            }

            fprintf(ctx->stream, "(*");
            return;
        }
        case TIR_STRUCT_TYPE: {
            fprintf(ctx->stream, "struct _S%s ", tir_get_str(ctx->tir, get_struct_type(ctx->tir, type).name));
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type_before(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type_before(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type_before(ctx, get_affine_elem_type(ctx->tir, type));
            return;
        }
        case TIR_TYPE_PARAMETER: {
            fprintf(ctx->stream, "void ");
            return;
        }
        default: {
            break;
        }
    }

    abort();
}

static void gen_type_after(GenContext *ctx, TirId type) {
    switch (get_term_tag(ctx->tir, type)) {
        case TIR_PRIMITIVE_TYPE:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_TYPE_PARAMETER:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_STRUCT_TYPE: {
            return;
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(ctx->tir, type);
            int64_t length = get_array_length_type(ctx->tir, array.index);
            fprintf(ctx->stream, "[%ld]", length);
            gen_type_after(ctx, array.elem);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            gen_ptr_type_after(ctx, ptype(VOID));
            return;
        }
        case TIR_FUNCTION_TYPE: {
            fprintf(ctx->stream, ")");
            gen_params(ctx, type);
            TirId ret = get_function_type(ctx->tir, type).ret;
            if (ret.id != TYPE_VOID) {
                if (is_type_passed_by_ptr(ctx, ret)) {
                    gen_ptr_type_after(ctx, ret);
                } else {
                    gen_type_after(ctx, ret);
                }
            }
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type_after(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type_after(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type_after(ctx, get_affine_elem_type(ctx->tir, type));
            return;
        }
        default: {
            abort();
        }
    }
}

static void gen_extern_var(GenContext *ctx, TirId value) {
    TirId type = get_value_type(ctx->tir, value);
    fprintf(ctx->stream, "extern ");
    gen_type_before(ctx, type);
    char const *name = get_value_str(ctx->tir, value);
    fprintf(ctx->stream, "%s", name);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ";\n");
}

static void gen_extern_function(GenContext *ctx, TirId value) {
    TirId type = get_value_type(ctx->tir, value);
    TirId ret_type = get_function_type(ctx->tir, type).ret;

    if (ret_type.id != TYPE_VOID) {
        gen_type_before(ctx, ret_type);
        if (is_type_passed_by_ptr(ctx, ret_type)) {
            if (ptr_type_needs_parens(ctx, ret_type)) {
                fprintf(ctx->stream, "(*");
            } else {
                fprintf(ctx->stream, "*");
            }
        }
    } else {
        fprintf(ctx->stream, "void ");
    }

    char const *name = get_value_str(ctx->tir, value);
    fprintf(ctx->stream, "%s", name);
    gen_params(ctx, type);

    if (ret_type.id != TYPE_VOID) {
        if (is_type_passed_by_ptr(ctx, ret_type)) {
            if (ptr_type_needs_parens(ctx, ret_type)) {
                fprintf(ctx->stream, ")");
            }
        }
        gen_type_after(ctx, ret_type);
    }

    fprintf(ctx->stream, ";\n");
}

static void gen_function_decl(GenContext *ctx, TirId value, bool is_main) {
    if (is_main) {
        fprintf(ctx->stream, "int main(void);\n");
        return;
    }

    TirId type = get_value_type(ctx->tir, value);
    TirId ret_type = get_function_type(ctx->tir, type).ret;
    fprintf(ctx->stream, "static ");

    if (ret_type.id != TYPE_VOID) {
        gen_type_before(ctx, ret_type);
        if (is_type_passed_by_ptr(ctx, ret_type)) {
            if (ptr_type_needs_parens(ctx, ret_type)) {
                fprintf(ctx->stream, "(*");
            } else {
                fprintf(ctx->stream, "*");
            }
        }
    } else {
        fprintf(ctx->stream, "void ");
    }

    char const *name = get_value_str(ctx->tir, value);
    fprintf(ctx->stream, "%s", name);
    gen_params(ctx, type);

    if (ret_type.id != TYPE_VOID) {
        if (is_type_passed_by_ptr(ctx, ret_type)) {
            if (ptr_type_needs_parens(ctx, ret_type)) {
                fprintf(ctx->stream, ")");
            }
        }
        gen_type_after(ctx, ret_type);
    }

    fprintf(ctx->stream, ";\n");
}

static void print_string(GenContext *ctx, char const *str) {
    fprintf(ctx->stream, "\"");
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] == '"') {
            fprintf(ctx->stream, "\\\"");
        } else if (str[i + 4] == '\\') {
            fprintf(ctx->stream, "\\\\");
        } else if (str[i + 4] >= 32 && str[i + 4] <= 126) {
            fprintf(ctx->stream, "%c", str[i + 4]);
        } else {
            fprintf(ctx->stream, "\\x%02X", (int) (unsigned char) str[i + 4]);
        }
    }
    fprintf(ctx->stream, "\"");
}

static void gen_value(GenContext *ctx, TirId value) {
    switch (get_term_tag(ctx->tir, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR: {
            char const *name = get_value_str(ctx->tir, value);
            fprintf(ctx->stream, "%s", name);
            break;
        }
        case TIR_STRING: {
            print_string(ctx, get_value_str(ctx->tir, value));
            break;
        }
        case TIR_CONST_INT: {
            fprintf(ctx->stream, "%ld", get_value_int(ctx->tir, value));
            break;
        }
        case TIR_CONST_FLOAT: {
            fprintf(ctx->stream, "%f", get_value_float(ctx->tir, value));
            break;
        }
        case TIR_CONST_NULL: {
            fprintf(ctx->stream, "0");
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            fprintf(ctx->stream, "v%d", get_term_data(ctx->tir, value)->b);
            break;
        }
        default: {
            abort();
        }
    }
}

static Operand new_tmp(
    GenContext *ctx,
    bool is_lvalue,
    TirId type
) {
    Operand operand = {
        .is_lvalue = is_lvalue,
        .tag = OPERAND_TMP,
        .type = type,
        .index = ctx->tmp_count++,
    };
    vec_push(&ctx->stack, operand);
    return operand;
}

static Operand introduce_temporary(
    GenContext *ctx,
    bool is_lvalue,
    TirId type
) {
    fprintf(ctx->stream, "    ");

    if (is_lvalue) {
        gen_ptr_type_before(ctx, type);
    } else {
        gen_type_before(ctx, type);
    }

    Operand a = new_tmp(ctx, is_lvalue, type);
    fprintf(ctx->stream, "t%d", a.index);

    if (is_lvalue) {
        gen_ptr_type_after(ctx, type);
    } else {
        gen_type_after(ctx, type);
    }

    fprintf(ctx->stream, " = ");
    return a;
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

static void gen_operand(GenContext *ctx, Operand *a) {
    if (a->is_lvalue) {
        fprintf(ctx->stream, "(*");
    }
    switch (a->tag) {
        case OPERAND_INT: {
            fprintf(ctx->stream, "%ld", a->i);
            break;
        }
        case OPERAND_TIR: {
            gen_value(ctx, a->value);
            break;
        }
        case OPERAND_TMP: {
            fprintf(ctx->stream, "t%d", a->index);
            break;
        }
    }
    if (a->is_lvalue) {
        fprintf(ctx->stream, ")");
    }
}

static Operand print_alloc(GenContext *ctx, TirId type) {
    if (type.id == TYPE_VOID) {
        abort();
    }

    fprintf(ctx->stream, "    ");
    gen_type_before(ctx, type);
    Operand a = new_tmp(ctx, MIR_ALLOC, type);
    gen_operand(ctx, &a);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ";\n");
    return a;
}

static void gen_alloc(GenContext *ctx) {
    TirId type = pop_term(ctx);
    print_alloc(ctx, type);
}

static void gen_alloc_var(GenContext *ctx) {
    TirId v = pop_term(ctx);
    fprintf(ctx->stream, "    ");
    TirId type = get_value_type(ctx->tir, v);
    gen_type_before(ctx, type);
    fprintf(ctx->stream, "v%d", get_term_data(ctx->tir, v)->b);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ";\n");
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_TIR,
        .type = type,
        .value = v,
    };
    vec_push(&ctx->stack, operand);
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
        case TIR_PARAMETER: {
            operand.is_lvalue = is_type_passed_by_ptr(ctx, get_value_type(ctx->tir, a));
            break;
        }
        case TIR_EXTERN_VAR:
        case TIR_STRING:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL: {
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
    introduce_temporary(ctx, false, type);
    fputs("&", ctx->stream);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ";\n");
}

static void gen_deref(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    introduce_temporary(ctx, true, remove_any_pointer(ctx->tir, a.type));
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ";\n");
}

static void gen_unary(GenContext *ctx, char const *op) {
    Operand a = pop_operand(ctx);
    introduce_temporary(ctx, false, a.type);
    fputs(op, ctx->stream);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ";\n");
}

static void gen_binary(GenContext *ctx, char const *op) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    introduce_temporary(ctx, false, a.type);
    gen_operand(ctx, &a);
    fprintf(ctx->stream, " %s ", op);
    gen_operand(ctx, &b);
    fprintf(ctx->stream, ";\n");
}

static void gen_bool_binary(GenContext *ctx, char const *op) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    introduce_temporary(ctx, false, ptype(bool));
    gen_operand(ctx, &a);
    fprintf(ctx->stream, " %s ", op);
    gen_operand(ctx, &b);
    fprintf(ctx->stream, ";\n");
}

static void gen_mod(GenContext *ctx) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);

    if (type_is_int(a.type)) {
        introduce_temporary(ctx, false, a.type);
        gen_operand(ctx, &a);
        fprintf(ctx->stream, " %% ");
        gen_operand(ctx, &b);
        fprintf(ctx->stream, ";\n");
    } else {
        introduce_temporary(ctx, false, a.type);
        fprintf(ctx->stream, "%s(", a.type.id == TYPE_f32 ? "__builtin_fmodf" : "__builtin_fmod");
        gen_operand(ctx, &a);
        fprintf(ctx->stream, ", ");
        gen_operand(ctx, &b);
        fprintf(ctx->stream, ");\n");
    }
}

static void copy_c_value(GenContext *ctx, Operand *dst, Operand *src) {
    if (get_term_tag(ctx->tir, src->type) == TIR_ARRAY_TYPE) {
        fprintf(ctx->stream, "    __builtin_memcpy(");
        if (dst) {
            fprintf(ctx->stream, "&");
            gen_operand(ctx, dst);
        } else {
            fprintf(ctx->stream, "ret");
        }
        fprintf(ctx->stream, ", &");
        gen_operand(ctx, src);
        fprintf(ctx->stream, ", %ld);\n", sizeof_type(ctx->tir, src->type, ctx->target));
    } else {
        fprintf(ctx->stream, "    ");
        if (dst) {
            gen_operand(ctx, dst);
        } else {
            fprintf(ctx->stream, "*ret");
        }
        fprintf(ctx->stream, " = ");
        gen_operand(ctx, src);
        fprintf(ctx->stream, ";\n");
    }
}

static void gen_assign(GenContext *ctx) {
    Operand b = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    copy_c_value(ctx, &a, &b);
}

static void gen_cast(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    TirId type = pop_term(ctx);
    introduce_temporary(ctx, false, type);
    fprintf(ctx->stream, "(");
    gen_type_before(ctx, type);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ") ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ";\n");
}

static void gen_zext(GenContext *ctx) {
    Operand a = pop_operand(ctx);
    TirId type = pop_term(ctx);
    introduce_temporary(ctx, false, type);
    fprintf(ctx->stream, "(");
    gen_type_before(ctx, type);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ") ");
    gen_operand(ctx, &a);

    int64_t int_size = sizeof_type(ctx->tir, a.type, ctx->target);
    uint64_t mask = ((uint64_t) 1 << (int_size * 8)) - 1;
    fprintf(ctx->stream, " & 0x%lX;\n", mask);
}

static void gen_call(GenContext *ctx) {
    TirId type = pop_term(ctx);
    FunctionType function_type = get_function_type(ctx->tir, type);
    int32_t arg_count = function_type.param_count;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_type_passed_by_ptr(ctx, function_type.ret);
    Operand a;

    if (implicit_return) {
        a = print_alloc(ctx, function_type.ret);
        fprintf(ctx->stream, "    ");
    } else if (function_type.ret.id != TYPE_VOID) {
        a = introduce_temporary(ctx, false, function_type.ret);
    } else {
        fprintf(ctx->stream, "    ");
    }

    int32_t stack_elems = 1 + arg_count + (function_type.ret.id != TYPE_VOID);
    Operand *f = ctx->stack.ptr + ctx->stack.len - stack_elems;
    Operand *args = f + 1;

    gen_operand(ctx, f);
    fprintf(ctx->stream, "(");

    if (implicit_return) {
        fprintf(ctx->stream, "&");
        gen_operand(ctx, &a);

        if (arg_count) {
            fprintf(ctx->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }

        if (is_type_passed_by_ptr(ctx, get_function_type_param(ctx->tir, type, i))) {
            fprintf(ctx->stream, "&");
        }
        gen_operand(ctx, &args[i]);
    }

    fprintf(ctx->stream, ");\n");
    assert(ctx->stack.len >= stack_elems);
    ctx->stack.len -= stack_elems;

    if (function_type.ret.id != TYPE_VOID) {
        vec_push(&ctx->stack, a);
    }
}

static void gen_index(GenContext *ctx) {
    Operand index = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    TirId elem_type = remove_c_pointer_like(ctx->tir, a.type);
    introduce_temporary(ctx, true, elem_type);
    fprintf(ctx->stream, "&((");
    gen_ptr_type_before(ctx, elem_type);
    gen_ptr_type_after(ctx, elem_type);
    fprintf(ctx->stream, ") ");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ")[");
    gen_operand(ctx, &index);
    fprintf(ctx->stream, "];\n");
}

static void gen_slice_index(GenContext *ctx) {
    Operand index = pop_operand(ctx);
    Operand a = pop_operand(ctx);
    TirId elem_type = remove_c_pointer_like(ctx->tir, a.type);
    introduce_temporary(ctx, true, elem_type);
    fprintf(ctx->stream, "&((");
    gen_ptr_type_before(ctx, elem_type);
    gen_ptr_type_after(ctx, elem_type);
    fprintf(ctx->stream, ") (");
    gen_operand(ctx, &a);
    fprintf(ctx->stream, ")._1)[");
    gen_operand(ctx, &index);
    fprintf(ctx->stream, "];\n");
}

static void gen_access(GenContext *ctx) {
    Operand s = pop_operand(ctx);
    int32_t field = pop_data(ctx);
    TirId field_type = get_any_struct_type_field(ctx->tir, s.type, field);
    introduce_temporary(ctx, true, field_type);
    fprintf(ctx->stream, "&((");
    gen_ptr_type_before(ctx, s.type);
    gen_ptr_type_after(ctx, s.type);
    fprintf(ctx->stream, ") &");
    gen_operand(ctx, &s);
    fprintf(ctx->stream, ")->_%d;\n", field);
}

static void gen_br(GenContext *ctx) {
    int32_t block = pop_data(ctx);
    fprintf(ctx->stream, "    goto L%d;\n", block);
}

static void gen_br_if(GenContext *ctx) {
    Operand condition = pop_operand(ctx);
    int32_t block = pop_data(ctx);
    fprintf(ctx->stream, "    if (");
    gen_operand(ctx, &condition);
    fprintf(ctx->stream, ") goto L%d;\n", block);
}

static void gen_br_if_not(GenContext *ctx) {
    Operand condition = pop_operand(ctx);
    int32_t block = pop_data(ctx);
    fprintf(ctx->stream, "    if (!");
    gen_operand(ctx, &condition);
    fprintf(ctx->stream, ") goto L%d;\n", block);
}

static void gen_ret_void(GenContext *ctx) {
    if (ctx->is_main) {
        fprintf(ctx->stream, "    return 0;\n");
    } else {
        fprintf(ctx->stream, "    return;\n");
    }
}

static void gen_ret(GenContext *ctx) {
    Operand a = pop_operand(ctx);

    if (!is_type_passed_by_ptr(ctx, ctx->return_type)) {
        fprintf(ctx->stream, "    return ");
        gen_operand(ctx, &a);
        fprintf(ctx->stream, ";\n");
    } else {
        copy_c_value(ctx, NULL, &a);
        fprintf(ctx->stream, "    return ret;\n");
    }
}

static void gen_instruction(GenContext *ctx, int32_t i) {
    MirTag tag = ctx->mir->insts.ptr[i];
    switch (tag) {
        case MIR_ALLOC: gen_alloc(ctx); break;
        case MIR_ALLOC_VAR: gen_alloc_var(ctx); break;
        case MIR_STACK_COPY: stack_copy(ctx); break;
        case MIR_STACK_COPY_AT: stack_copy_at(ctx); break;
        case MIR_STACK_POP: stack_pop(ctx); break;
        case MIR_INT: gen_int(ctx); break;
        case MIR_TIR_VALUE: gen_tir_value(ctx); break;
        case MIR_ASSIGN: gen_assign(ctx); break;
        case MIR_NEG: gen_unary(ctx, "-"); break;
        case MIR_NOT: gen_unary(ctx, "!"); break;
        case MIR_ADDRESS: gen_address(ctx); break;
        case MIR_DEREF: gen_deref(ctx); break;
        case MIR_ADD: gen_binary(ctx, "+"); break;
        case MIR_SUB: gen_binary(ctx, "-"); break;
        case MIR_MUL: gen_binary(ctx, "*"); break;
        case MIR_DIV: gen_binary(ctx, "/"); break;
        case MIR_MOD: gen_mod(ctx); break;
        case MIR_AND: gen_binary(ctx, "&"); break;
        case MIR_OR: gen_binary(ctx, "|"); break;
        case MIR_XOR: gen_binary(ctx, "^"); break;
        case MIR_SHL: gen_binary(ctx, "<<"); break;
        case MIR_SHR: gen_binary(ctx, ">>"); break;
        case MIR_EQ: gen_bool_binary(ctx, "=="); break;
        case MIR_NE: gen_bool_binary(ctx, "!="); break;
        case MIR_LT: gen_bool_binary(ctx, "<"); break;
        case MIR_GT: gen_bool_binary(ctx, ">"); break;
        case MIR_LE: gen_bool_binary(ctx, "<="); break;
        case MIR_GE: gen_bool_binary(ctx, ">="); break;

        case MIR_ITOF:
        case MIR_ITRUNC:
        case MIR_SEXT:
        case MIR_FTOI:
        case MIR_FTRUNC:
        case MIR_FEXT:
        case MIR_PTR_CAST: {
            gen_cast(ctx);
            break;
        }
        case MIR_ZEXT: gen_zext(ctx); break;
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
    bool is_main = input->global_deps.main.id == value.id;

    ctx->tir.thread = &input->insts[f_index].deps;
    TirId type = get_value_type(ctx->tir, value);
    TirId ret_type = get_function_type(ctx->tir, type).ret;
    ctx->return_type = ret_type;
    ctx->is_main = is_main;
    ctx->data_top = input->mir_result->data_starts[f_index];
    ctx->tmp_count = 0;
    ctx->stack.len = 0;

    if (is_main) {
        fprintf(ctx->stream, "int main(void)");
    } else {
        fprintf(ctx->stream, "static ");

        if (ret_type.id != TYPE_VOID) {
            gen_type_before(ctx, ret_type);

            if (is_type_passed_by_ptr(ctx, ret_type)) {
                if (ptr_type_needs_parens(ctx, ret_type)) {
                    fprintf(ctx->stream, "(*");
                } else {
                    fprintf(ctx->stream, "*");
                }
            }
        } else {
            fprintf(ctx->stream, "void ");
        }

        char const *name = get_value_str(ctx->tir, value);
        fprintf(ctx->stream, "%s", name);
        gen_params(ctx, type);

        if (ret_type.id != TYPE_VOID) {
            if (is_type_passed_by_ptr(ctx, ret_type)) {
                if (ptr_type_needs_parens(ctx, ret_type)) {
                    fprintf(ctx->stream, ")");
                }
            }

            gen_type_after(ctx, ret_type);
        }
    }

    fprintf(ctx->stream, " {\n");
    int blocks = 1;

    for (int32_t i = mir_start; i < mir_end; i++) {
        if (i != mir_start && is_mir_terminator(ctx->mir->insts.ptr[i - 1])) {
            fprintf(ctx->stream, "L%d:\n    ;\n", blocks++);
        }

        gen_instruction(ctx, i);
    }

    assert(ctx->stack.len == 0);
    assert(
        f_index == input->global_deps.functions.len - 1
            ? (ctx->data_top == input->mir_result->mir.data.len)
            : (ctx->data_top == input->mir_result->data_starts[f_index + 1])
    );
    fprintf(ctx->stream, "}\n");
}

static void gen_struct_decl(GenContext *ctx, TirId type) {
    TaggedType t = get_tagged_type(ctx->tir, type);
    fprintf(ctx->stream, "struct _S%s;\n", tir_get_str(ctx->tir, t.name));
}

static void gen_struct(GenContext *ctx, TirId type) {
    TaggedType t = get_tagged_type(ctx->tir, type);
    StructType s = get_struct_type(ctx->tir, t.inner);
    fprintf(ctx->stream, "struct _S%s {\n", tir_get_str(ctx->tir, t.name));

    for (int32_t i = 0; i < s.field_count; i++) {
        TirId field_type = s.fields[i];
        fprintf(ctx->stream, "    ");
        gen_type_before(ctx, field_type);
        fprintf(ctx->stream, "_%d", i);
        gen_type_after(ctx, field_type);
        fprintf(ctx->stream, ";\n");
    }

    fprintf(ctx->stream, "};\n");
}

static void gen_thread(GenContext *ctx, Tir *tir) {
    ctx->tir.thread = tir;

    for (int32_t i = 0; i < tir->structs.len; i++) {
        TirId type = tir->structs.ptr[i];
        gen_struct_decl(ctx, type);
    }

    for (int32_t i = 0; i < tir->structs.len; i++) {
        TirId type = tir->structs.ptr[i];
        gen_struct(ctx, type);
    }

    for (int32_t i = 0; i < tir->extern_vars.len; i++) {
        TirId value = tir->extern_vars.ptr[i];
        gen_extern_var(ctx, value);
    }

    for (int32_t i = 0; i < tir->extern_functions.len; i++) {
        TirId value = tir->extern_functions.ptr[i];
        gen_extern_function(ctx, value);
    }
}

void gen_c(GenInput *input, Target target) {
    FILE *stream = fopen("a.c", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "#include <stdint.h>\n\n");
    int ptr_bits = sizeof_pointer(target) * 8;
    fprintf(stream, "struct Slice { int%d_t _0; void *_1; };\n\n", ptr_bits);

    GenContext ctx = {
        .target = target,
        .tir = {
            .global = &input->global_deps,
            .thread = NULL,
        },
        .mir = &input->mir_result->mir,
        .stream = stream,
    };

    gen_thread(&ctx, &input->global_deps);
    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_thread(&ctx, &input->insts[i].deps);
    }

    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        TirId value = input->global_deps.functions.ptr[i];
        gen_function_decl(&ctx, value, input->global_deps.main.id == value.id);
    }

    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_function(&ctx, input, i);
    }

    fclose(stream);
}
