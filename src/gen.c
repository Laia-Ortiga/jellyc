#include "gen.h"

#include "mir.h"
#include "tir.h"
#include "type.h"
#include "fwd.h"
#include "util.h"

#include <assert.h>
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
    TirId return_type;
    bool is_main;
    Target target;
    TirContext tir;
    int32_t thread;
    FILE *stream;
} GenContext;

static void gen_type_before(GenContext *c, TirId type);
static void gen_type_after(GenContext *c, TirId type);

static bool ptr_type_needs_parens(GenContext *c, TirId type) {
    return get_term_tag(c->tir, type) == TIR_ARRAY_TYPE || get_term_tag(c->tir, type) == TIR_FUNCTION_TYPE;
}

static void gen_ptr_type_before(GenContext *c, TirId type) {
    gen_type_before(c, type);

    if (ptr_type_needs_parens(c, type)) {
        fprintf(c->stream, "(*");
    } else {
        fprintf(c->stream, "*");
    }
}

static void gen_ptr_type_after(GenContext *c, TirId type) {
    if (ptr_type_needs_parens(c, type)) {
        fprintf(c->stream, ")");
    }

    gen_type_after(c, type);
}

static bool is_type_passed_by_ptr(GenContext *c, TirId type) {
    if (is_aggregate_type(c->tir, type)) {
        return true;
    }

    return false;
}

static void gen_params(GenContext *c, TirId type) {
    fprintf(c->stream, "(");
    TirFunctionType func_type = tir_get_function_type(c->tir, type);
    bool c_has_params = func_type.params.len != 0;

    if (func_type.ret.id != TYPE_VOID && is_type_passed_by_ptr(c, func_type.ret)) {
        gen_ptr_type_before(c, func_type.ret);
        fprintf(c->stream, "ret");
        gen_ptr_type_after(c, func_type.ret);

        if (c_has_params) {
            fprintf(c->stream, ", ");
        }

        c_has_params = true;
    }

    if (c_has_params) {
        for (int32_t i = 0; i < func_type.params.len; i++) {
            if (i != 0) {
                fprintf(c->stream, ", ");
            }

            TirId param_type = get_function_type_param(c->tir, type, i);
            if (is_type_passed_by_ptr(c, param_type)) {
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
}

static void gen_struct_name(GenContext *c, TirId type) {
    TirStructType s = tir_get_struct_type(c->tir, type);
    if (tir_get_storage(c->tir, type) == c->tir.global) {
        fprintf(c->stream, "struct _S%d_%d_%s", 0, type.id, tir_get_str(c->tir, s.name));
    } else {
        fprintf(c->stream, "struct _S%d_%d_%s", c->thread, type.id, tir_get_str(c->tir, s.name));
    }
}

static void gen_type_before(GenContext *c, TirId type) {
    switch (get_term_tag(c->tir, type)) {
        case TIR_RESERVED: {
            switch ((ReservedTerm) type.id) {
                case TYPE_VOID: fprintf(c->stream, "void "); return;

                case TYPE_i8: fprintf(c->stream, "int8_t "); return;
                case TYPE_i16: fprintf(c->stream, "int16_t "); return;
                case TYPE_i32: fprintf(c->stream, "int32_t "); return;
                case TYPE_i64: fprintf(c->stream, "int64_t "); return;

                case TYPE_isize: fprintf(c->stream, "int%d_t ", sizeof_pointer(c->target) * 8); return;

                case TYPE_f32: fprintf(c->stream, "float "); return;
                case TYPE_f64: fprintf(c->stream, "double "); return;

                case TYPE_byte: fprintf(c->stream, "char "); return;

                case TYPE_bool: fprintf(c->stream, "unsigned char "); return;

                default: break;
            }
            break;
        }
        case TIR_ARRAY_TYPE:{
            gen_type_before(c, tir_get_array_type(c->tir, type).elem);
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            fprintf(c->stream, "int%d_t ", sizeof_pointer(c->target) * 8);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            gen_ptr_type_before(c, ptype(VOID));
            return;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            fprintf(c->stream, "struct Slice ");
            return;
        }
        case TIR_FUNCTION_TYPE: {
            TirId ret = tir_get_function_type(c->tir, type).ret;

            if (ret.id != TYPE_VOID) {
                if (is_type_passed_by_ptr(c, ret)) {
                    gen_ptr_type_before(c, ret);
                } else {
                    gen_type_before(c, ret);
                }
            } else {
                fprintf(c->stream, "void ");
            }

            fprintf(c->stream, "(*");
            return;
        }
        case TIR_STRUCT_TYPE: {
            gen_struct_name(c, type);
            fprintf(c->stream, " ");
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type_before(c, tir_get_enum_type(c->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type_before(c, tir_get_tagged_type(c->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type_before(c, tir_get_affine_type(c->tir, type).elem);
            return;
        }
        case TIR_TYPE_PARAMETER: {
            fprintf(c->stream, "void ");
            return;
        }
        default: {
            break;
        }
    }

    abort();
}

static void gen_type_after(GenContext *c, TirId type) {
    switch (get_term_tag(c->tir, type)) {
        case TIR_RESERVED:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_TYPE_PARAMETER:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_STRUCT_TYPE: {
            return;
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType array = tir_get_array_type(c->tir, type);
            int64_t length = tir_get_array_length_type(c->tir, array.index).length;
            fprintf(c->stream, "[%ld]", length);
            gen_type_after(c, array.elem);
            return;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            gen_ptr_type_after(c, ptype(VOID));
            return;
        }
        case TIR_FUNCTION_TYPE: {
            fprintf(c->stream, ")");
            gen_params(c, type);
            TirId ret = tir_get_function_type(c->tir, type).ret;
            if (ret.id != TYPE_VOID) {
                if (is_type_passed_by_ptr(c, ret)) {
                    gen_ptr_type_after(c, ret);
                } else {
                    gen_type_after(c, ret);
                }
            }
            return;
        }
        case TIR_ENUM_TYPE: {
            gen_type_after(c, tir_get_enum_type(c->tir, type).repr);
            return;
        }
        case TIR_TAGGED_TYPE: {
            gen_type_after(c, tir_get_tagged_type(c->tir, type).inner);
            return;
        }
        case TIR_AFFINE_TYPE: {
            gen_type_after(c, tir_get_affine_type(c->tir, type).elem);
            return;
        }
        default: {
            abort();
        }
    }
}

static void gen_extern_var(GenContext *c, TirId value) {
    TirExternVar t = tir_get_extern_var(c->tir, value);
    fprintf(c->stream, "extern ");
    gen_type_before(c, t.type);
    char const *name = tir_get_str(c->tir, t.name);
    fprintf(c->stream, "%s", name);
    gen_type_after(c, t.type);
    fprintf(c->stream, ";\n");
}

static void gen_extern_function(GenContext *c, TirId value) {
    TirExternFunction t = tir_get_extern_function(c->tir, value);
    TirId ret_type = tir_get_function_type(c->tir, t.type).ret;

    if (ret_type.id != TYPE_VOID) {
        gen_type_before(c, ret_type);
        if (is_type_passed_by_ptr(c, ret_type)) {
            if (ptr_type_needs_parens(c, ret_type)) {
                fprintf(c->stream, "(*");
            } else {
                fprintf(c->stream, "*");
            }
        }
    } else {
        fprintf(c->stream, "void ");
    }

    char const *name = tir_get_str(c->tir, t.name);
    fprintf(c->stream, "%s", name);
    gen_params(c, t.type);

    if (ret_type.id != TYPE_VOID) {
        if (is_type_passed_by_ptr(c, ret_type)) {
            if (ptr_type_needs_parens(c, ret_type)) {
                fprintf(c->stream, ")");
            }
        }
        gen_type_after(c, ret_type);
    }

    fprintf(c->stream, ";\n");
}

static void gen_function_decl(GenContext *c, TirId value, bool is_main) {
    if (is_main) {
        fprintf(c->stream, "int main(void);\n");
        return;
    }

    TirFunction t = tir_get_function(c->tir, value);
    TirId ret_type = tir_get_function_type(c->tir, t.type).ret;
    fprintf(c->stream, "static ");

    if (ret_type.id != TYPE_VOID) {
        gen_type_before(c, ret_type);
        if (is_type_passed_by_ptr(c, ret_type)) {
            if (ptr_type_needs_parens(c, ret_type)) {
                fprintf(c->stream, "(*");
            } else {
                fprintf(c->stream, "*");
            }
        }
    } else {
        fprintf(c->stream, "void ");
    }

    char const *name = tir_get_str(c->tir, t.name);
    fprintf(c->stream, "%s", name);
    gen_params(c, t.type);

    if (ret_type.id != TYPE_VOID) {
        if (is_type_passed_by_ptr(c, ret_type)) {
            if (ptr_type_needs_parens(c, ret_type)) {
                fprintf(c->stream, ")");
            }
        }
        gen_type_after(c, ret_type);
    }

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

static void gen_value(GenContext *c, TirId value) {
    switch (get_term_tag(c->tir, value)) {
        case TIR_FUNCTION: {
            TirFunction t = tir_get_function(c->tir, value);
            fprintf(c->stream, "%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_EXTERN_FUNCTION: {
            TirExternFunction t = tir_get_extern_function(c->tir, value);
            fprintf(c->stream, "%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_EXTERN_VAR: {
            TirExternVar t = tir_get_extern_var(c->tir, value);
            fprintf(c->stream, "%s", tir_get_str(c->tir, t.name));
            break;
        }
        case TIR_STRING: {
            TirString t = tir_get_string(c->tir, value);
            print_string(c, tir_get_str(c->tir, t.value));
            break;
        }
        case TIR_INT: {
            fprintf(c->stream, "%ld", tir_get_int(c->tir, value).value);
            break;
        }
        case TIR_FLOAT: {
            fprintf(c->stream, "%f", tir_get_float(c->tir, value).value);
            break;
        }
        case TIR_NULL: {
            fprintf(c->stream, "0");
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            fprintf(c->stream, "v%d", tir_get_variable(c->tir, value).index);
            break;
        }
        default: {
            abort();
        }
    }
}

static Operand new_tmp(
    GenContext *c,
    bool is_lvalue,
    TirId type
) {
    Operand operand = {
        .is_lvalue = is_lvalue,
        .tag = OPERAND_TMP,
        .type = type,
        .index = c->tmp_count++,
    };
    vec_push(&c->stack, operand);
    return operand;
}

static Operand introduce_temporary(
    GenContext *c,
    bool is_lvalue,
    TirId type
) {
    fprintf(c->stream, "    ");

    if (is_lvalue) {
        gen_ptr_type_before(c, type);
    } else {
        gen_type_before(c, type);
    }

    Operand a = new_tmp(c, is_lvalue, type);
    fprintf(c->stream, "t%d", a.index);

    if (is_lvalue) {
        gen_ptr_type_after(c, type);
    } else {
        gen_type_after(c, type);
    }

    fprintf(c->stream, " = ");
    return a;
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

static void gen_operand(GenContext *c, Operand *a) {
    if (a->is_lvalue) {
        fprintf(c->stream, "(*");
    }
    switch (a->tag) {
        case OPERAND_INT: {
            fprintf(c->stream, "%ld", a->i);
            break;
        }
        case OPERAND_TIR: {
            gen_value(c, a->value);
            break;
        }
        case OPERAND_TMP: {
            fprintf(c->stream, "t%d", a->index);
            break;
        }
    }
    if (a->is_lvalue) {
        fprintf(c->stream, ")");
    }
}

static Operand print_alloc(GenContext *c, TirId type) {
    if (type.id == TYPE_VOID) {
        abort();
    }

    fprintf(c->stream, "    ");
    gen_type_before(c, type);
    Operand a = new_tmp(c, MIR_ALLOC, type);
    gen_operand(c, &a);
    gen_type_after(c, type);
    fprintf(c->stream, ";\n");
    return a;
}

static void gen_alloc(GenContext *c) {
    TirId type = pop_term(c);
    print_alloc(c, type);
}

static void gen_alloc_var(GenContext *c) {
    TirId v = pop_term(c);
    fprintf(c->stream, "    ");
    TirId type = get_value_type(c->tir, v);
    gen_type_before(c, type);
    fprintf(c->stream, "v%d", tir_get_variable(c->tir, v).index);
    gen_type_after(c, type);
    fprintf(c->stream, ";\n");
    Operand operand = {
        .is_lvalue = false,
        .tag = OPERAND_TIR,
        .type = type,
        .value = v,
    };
    vec_push(&c->stack, operand);
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
    switch (get_term_tag(c->tir, a)) {
        case TIR_PARAMETER: {
            operand.is_lvalue = is_type_passed_by_ptr(c, get_value_type(c->tir, a));
            break;
        }
        case TIR_EXTERN_VAR:
        case TIR_STRING:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_INT:
        case TIR_FLOAT:
        case TIR_NULL: {
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
    introduce_temporary(c, false, type);
    fputs("&", c->stream);
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_deref(GenContext *c) {
    Operand a = pop_operand(c);
    introduce_temporary(c, true, remove_any_pointer(c->tir, a.type));
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_unary(GenContext *c, char const *op) {
    Operand a = pop_operand(c);
    introduce_temporary(c, false, a.type);
    fputs(op, c->stream);
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_binary(GenContext *c, char const *op) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    introduce_temporary(c, false, a.type);
    gen_operand(c, &a);
    fprintf(c->stream, " %s ", op);
    gen_operand(c, &b);
    fprintf(c->stream, ";\n");
}

static void gen_bool_binary(GenContext *c, char const *op) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    introduce_temporary(c, false, ptype(bool));
    gen_operand(c, &a);
    fprintf(c->stream, " %s ", op);
    gen_operand(c, &b);
    fprintf(c->stream, ";\n");
}

static void gen_mod(GenContext *c) {
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);

    if (type_is_int(a.type)) {
        introduce_temporary(c, false, a.type);
        gen_operand(c, &a);
        fprintf(c->stream, " %% ");
        gen_operand(c, &b);
        fprintf(c->stream, ";\n");
    } else {
        introduce_temporary(c, false, a.type);
        fprintf(c->stream, "%s(", a.type.id == TYPE_f32 ? "__builtin_fmodf" : "__builtin_fmod");
        gen_operand(c, &a);
        fprintf(c->stream, ", ");
        gen_operand(c, &b);
        fprintf(c->stream, ");\n");
    }
}

static void copy_c_value(GenContext *c, Operand *dst, Operand *src) {
    if (get_term_tag(c->tir, src->type) == TIR_ARRAY_TYPE) {
        fprintf(c->stream, "    __builtin_memcpy(");
        if (dst) {
            fprintf(c->stream, "&");
            gen_operand(c, dst);
        } else {
            fprintf(c->stream, "ret");
        }
        fprintf(c->stream, ", &");
        gen_operand(c, src);
        fprintf(c->stream, ", %ld);\n", sizeof_type(c->tir, src->type, c->target));
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
    Operand b = pop_operand(c);
    Operand a = pop_operand(c);
    copy_c_value(c, &a, &b);
}

static void gen_cast(GenContext *c) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    introduce_temporary(c, false, type);
    fprintf(c->stream, "(");
    gen_type_before(c, type);
    gen_type_after(c, type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);
    fprintf(c->stream, ";\n");
}

static void gen_zext(GenContext *c) {
    Operand a = pop_operand(c);
    TirId type = pop_term(c);
    introduce_temporary(c, false, type);
    fprintf(c->stream, "(");
    gen_type_before(c, type);
    gen_type_after(c, type);
    fprintf(c->stream, ") ");
    gen_operand(c, &a);

    int64_t int_size = sizeof_type(c->tir, a.type, c->target);
    uint64_t mask = ((uint64_t) 1 << (int_size * 8)) - 1;
    fprintf(c->stream, " & 0x%lX;\n", mask);
}

static void gen_call(GenContext *c) {
    TirId type = pop_term(c);
    TirFunctionType function_type = tir_get_function_type(c->tir, type);
    int32_t arg_count = function_type.params.len;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_type_passed_by_ptr(c, function_type.ret);
    Operand a;

    if (implicit_return) {
        a = print_alloc(c, function_type.ret);
        fprintf(c->stream, "    ");
    } else if (function_type.ret.id != TYPE_VOID) {
        a = introduce_temporary(c, false, function_type.ret);
    } else {
        fprintf(c->stream, "    ");
    }

    int32_t stack_elems = 1 + arg_count + (function_type.ret.id != TYPE_VOID);
    Operand *f = c->stack.ptr + c->stack.len - stack_elems;
    Operand *args = f + 1;

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

        if (is_type_passed_by_ptr(c, get_function_type_param(c->tir, type, i))) {
            fprintf(c->stream, "&");
        }
        gen_operand(c, &args[i]);
    }

    fprintf(c->stream, ");\n");
    assert(c->stack.len >= stack_elems);
    c->stack.len -= stack_elems;

    if (function_type.ret.id != TYPE_VOID) {
        vec_push(&c->stack, a);
    }
}

static void gen_index(GenContext *c) {
    Operand index = pop_operand(c);
    Operand a = pop_operand(c);
    TirId elem_type = remove_c_pointer_like(c->tir, a.type);
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
    Operand index = pop_operand(c);
    Operand a = pop_operand(c);
    TirId elem_type = remove_c_pointer_like(c->tir, a.type);
    introduce_temporary(c, true, elem_type);
    fprintf(c->stream, "&((");
    gen_ptr_type_before(c, elem_type);
    gen_ptr_type_after(c, elem_type);
    fprintf(c->stream, ") (");
    gen_operand(c, &a);
    fprintf(c->stream, ")._1)[");
    gen_operand(c, &index);
    fprintf(c->stream, "];\n");
}

static void gen_access(GenContext *c) {
    Operand s = pop_operand(c);
    int32_t field = pop_data(c);
    TirId field_type = get_any_struct_type_field(c->tir, s.type, field);
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
    Operand condition = pop_operand(c);
    int32_t block = pop_data(c);
    fprintf(c->stream, "    if (");
    gen_operand(c, &condition);
    fprintf(c->stream, ") goto L%d;\n", block);
}

static void gen_br_if_not(GenContext *c) {
    Operand condition = pop_operand(c);
    int32_t block = pop_data(c);
    fprintf(c->stream, "    if (!");
    gen_operand(c, &condition);
    fprintf(c->stream, ") goto L%d;\n", block);
}

static void gen_ret_void(GenContext *c) {
    if (c->is_main) {
        fprintf(c->stream, "    return 0;\n");
    } else {
        fprintf(c->stream, "    return;\n");
    }
}

static void gen_ret(GenContext *c) {
    Operand a = pop_operand(c);

    if (!is_type_passed_by_ptr(c, c->return_type)) {
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
    switch (tag) {
        case MIR_ALLOC: gen_alloc(c); break;
        case MIR_ALLOC_VAR: gen_alloc_var(c); break;
        case MIR_STACK_COPY: stack_copy(c); break;
        case MIR_STACK_COPY_AT: stack_copy_at(c); break;
        case MIR_STACK_POP: stack_pop(c); break;
        case MIR_INT: gen_int(c); break;
        case MIR_TIR_VALUE: gen_tir_value(c); break;
        case MIR_ASSIGN: gen_assign(c); break;
        case MIR_NEG: gen_unary(c, "-"); break;
        case MIR_NOT: gen_unary(c, "!"); break;
        case MIR_ADDRESS: gen_address(c); break;
        case MIR_DEREF: gen_deref(c); break;
        case MIR_ADD: gen_binary(c, "+"); break;
        case MIR_SUB: gen_binary(c, "-"); break;
        case MIR_MUL: gen_binary(c, "*"); break;
        case MIR_DIV: gen_binary(c, "/"); break;
        case MIR_MOD: gen_mod(c); break;
        case MIR_AND: gen_binary(c, "&"); break;
        case MIR_OR: gen_binary(c, "|"); break;
        case MIR_XOR: gen_binary(c, "^"); break;
        case MIR_SHL: gen_binary(c, "<<"); break;
        case MIR_SHR: gen_binary(c, ">>"); break;
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
        case MIR_FEXT:
        case MIR_PTR_CAST: {
            gen_cast(c);
            break;
        }
        case MIR_ZEXT: gen_zext(c); break;
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
    bool is_main = input->global_deps.main.id == value.id;

    c->tir.thread = &input->insts[f_index].deps;
    TirFunction t = tir_get_function(c->tir, value);
    TirId ret_type = tir_get_function_type(c->tir, t.type).ret;
    c->return_type = ret_type;
    c->is_main = is_main;
    c->data_top = input->mir_result->data_starts[f_index];
    c->tmp_count = 0;
    c->stack.len = 0;

    if (is_main) {
        fprintf(c->stream, "int main(void)");
    } else {
        fprintf(c->stream, "static ");

        if (ret_type.id != TYPE_VOID) {
            gen_type_before(c, ret_type);

            if (is_type_passed_by_ptr(c, ret_type)) {
                if (ptr_type_needs_parens(c, ret_type)) {
                    fprintf(c->stream, "(*");
                } else {
                    fprintf(c->stream, "*");
                }
            }
        } else {
            fprintf(c->stream, "void ");
        }

        char const *name = tir_get_str(c->tir, t.name);
        fprintf(c->stream, "%s", name);
        gen_params(c, t.type);

        if (ret_type.id != TYPE_VOID) {
            if (is_type_passed_by_ptr(c, ret_type)) {
                if (ptr_type_needs_parens(c, ret_type)) {
                    fprintf(c->stream, ")");
                }
            }

            gen_type_after(c, ret_type);
        }
    }

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
        f_index == input->global_deps.functions.len - 1
            ? (c->data_top == input->mir_result->mir.data.len)
            : (c->data_top == input->mir_result->data_starts[f_index + 1])
    );
    fprintf(c->stream, "}\n");
}

static void gen_struct_decl(GenContext *c, TirId type) {
    TirTaggedType t = tir_get_tagged_type(c->tir, type);
    gen_struct_name(c, t.inner);
    fprintf(c->stream, ";\n");
}

static void gen_struct(GenContext *c, TirId type) {
    TirTaggedType t = tir_get_tagged_type(c->tir, type);
    TirStructType s = tir_get_struct_type(c->tir, t.inner);
    gen_struct_name(c, t.inner);
    fprintf(c->stream, " {\n");

    for (int32_t i = 0; i < s.fields.len; i++) {
        TirId field_type = s.fields.ptr[i];
        fprintf(c->stream, "    ");
        gen_type_before(c, field_type);
        fprintf(c->stream, "_%d", i);
        gen_type_after(c, field_type);
        fprintf(c->stream, ";\n");
    }

    fprintf(c->stream, "};\n");
}

static void gen_thread(GenContext *c, int32_t thread, Tir *tir) {
    c->thread = thread;
    c->tir.thread = tir;

    for (int32_t i = 0; i < tir->structs.len; i++) {
        TirId type = tir->structs.ptr[i];
        gen_struct_decl(c, type);
    }

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

void gen_c(GenInput *input, Target target) {
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
        TirId value = input->global_deps.functions.ptr[i];
        gen_function_decl(&c, value, input->global_deps.main.id == value.id);
    }

    for (int32_t i = 0; i < input->global_deps.functions.len; i++) {
        gen_function(&c, input, i);
    }

    fclose(stream);
}
