#include "gen.h"

#include "data/mir.h"
#include "data/tir.h"
#include "fwd.h"

#include <stdlib.h>

typedef struct {
    Mir *mir;
    int32_t mir_start;
    TypeId return_type;
    bool is_main;
    Target target;
    TirContext tir;
    FILE *stream;
} GenContext;

static void gen_type_before(GenContext *ctx, TypeId type);
static void gen_type_after(GenContext *ctx, TypeId type);

static bool ptr_type_needs_parens(GenContext *ctx, TypeId type) {
    return get_type_tag(ctx->tir, type) == TYPE_ARRAY || get_type_tag(ctx->tir, type) == TYPE_FUNCTION;
}

static void gen_ptr_type_before(GenContext *ctx, TypeId type) {
    gen_type_before(ctx, type);

    if (ptr_type_needs_parens(ctx, type)) {
        fprintf(ctx->stream, "(*");
    } else {
        fprintf(ctx->stream, "*");
    }
}

static void gen_ptr_type_after(GenContext *ctx, TypeId type) {
    if (ptr_type_needs_parens(ctx, type)) {
        fprintf(ctx->stream, ")");
    }

    gen_type_after(ctx, type);
}

static bool is_type_passed_by_ptr(GenContext *ctx, TypeId type) {
    if (is_aggregate_type(ctx->tir, type)) {
        return true;
    }

    return false;
}

static void gen_params(GenContext *ctx, TypeId type) {
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

            TypeId param_type = get_function_type_param(ctx->tir, type, i);
            if (type_is_unknown_size(ctx->tir, param_type)) {
                fprintf(ctx->stream, "void *t%d", i);
            } else {
                gen_type_before(ctx, param_type);
                fprintf(ctx->stream, "t%d", i);
                gen_type_after(ctx, param_type);
            }
        }
    } else {
        fprintf(ctx->stream, "void");
    }

    fprintf(ctx->stream, ")");
}

static void gen_type_before(GenContext *ctx, TypeId type) {
    switch (get_type_tag(ctx->tir, type)) {
        case TYPE_PRIMITIVE: {
            switch ((PrimitiveType) type.id) {
                case TYPE_INVALID:
                case TYPE_VOID:
                case TYPE_COUNT: break;

                case TYPE_i8: fprintf(ctx->stream, "int8_t "); return;
                case TYPE_i16: fprintf(ctx->stream, "int16_t "); return;
                case TYPE_i32: fprintf(ctx->stream, "int32_t "); return;
                case TYPE_i64: fprintf(ctx->stream, "int64_t "); return;

                case TYPE_SIZE_TAG:
                case TYPE_ALIGNMENT_TAG:
                case TYPE_isize: fprintf(ctx->stream, "int%d_t ", sizeof_pointer(ctx->target) * 8); return;

                case TYPE_f32: fprintf(ctx->stream, "float "); return;
                case TYPE_f64: fprintf(ctx->stream, "double "); return;

                case TYPE_char:
                case TYPE_byte: fprintf(ctx->stream, "char "); return;

                case TYPE_bool: fprintf(ctx->stream, "unsigned char "); return;
            }
            break;
        }
        case TYPE_ARRAY:{
            gen_type_before(ctx, get_array_type(ctx->tir, type).elem);
            return;
        }
        case TYPE_ARRAY_LENGTH: {
            fprintf(ctx->stream, "int%d_t ", sizeof_pointer(ctx->target) * 8);
            return;
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT: {
            gen_ptr_type_before(ctx, remove_pointer(ctx->tir, type));
            return;
        }
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: {
            fprintf(ctx->stream, "struct Slice ");
            return;
        }
        case TYPE_FUNCTION: {
            TypeId ret = get_function_type(ctx->tir, type).ret;

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
        case TYPE_STRUCT: {
            fprintf(ctx->stream, "struct _S%s ", ctx->tir.global->strtab.ptr + get_struct_type(ctx->tir, type).name);
            return;
        }
        case TYPE_ENUM: {
            gen_type_before(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TYPE_NEWTYPE: {
            gen_type_before(ctx, get_newtype_type(ctx->tir, type).type);
            return;
        }
        case TYPE_TAGGED: {
            gen_type_before(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TYPE_LINEAR: {
            gen_type_before(ctx, get_linear_elem_type(ctx->tir, type));
            return;
        }
        case TYPE_TYPE_PARAMETER: {
            fprintf(ctx->stream, "void ");
            return;
        }
    }
    abort();
}

static void gen_type_after(GenContext *ctx, TypeId type) {
    switch (get_type_tag(ctx->tir, type)) {
        case TYPE_PRIMITIVE:
        case TYPE_ARRAY_LENGTH:
        case TYPE_TYPE_PARAMETER:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT:
        case TYPE_STRUCT: {
            return;
        }
        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx->tir, type);
            int64_t length = get_array_length_type(ctx->tir, array.index);
            fprintf(ctx->stream, "[%ld]", length);
            gen_type_after(ctx, array.elem);
            return;
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT: {
            gen_ptr_type_after(ctx, remove_pointer(ctx->tir, type));
            return;
        }
        case TYPE_FUNCTION: {
            fprintf(ctx->stream, ")");
            gen_params(ctx, type);
            TypeId ret = get_function_type(ctx->tir, type).ret;
            if (ret.id != TYPE_VOID) {
                if (is_type_passed_by_ptr(ctx, ret)) {
                    gen_ptr_type_after(ctx, ret);
                } else {
                    gen_type_after(ctx, ret);
                }
            }
            return;
        }
        case TYPE_ENUM: {
            gen_type_after(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TYPE_NEWTYPE: {
            gen_type_after(ctx, get_newtype_type(ctx->tir, type).type);
            return;
        }
        case TYPE_TAGGED: {
            gen_type_after(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TYPE_LINEAR: {
            gen_type_after(ctx, get_linear_elem_type(ctx->tir, type));
            return;
        }
    }
    abort();
}

static void gen_extern_var(GenContext *ctx, ValueId value) {
    TypeId type = get_value_type(ctx->tir, value);
    fprintf(ctx->stream, "extern ");
    gen_type_before(ctx, type);
    int32_t name = get_value_data(ctx->tir, value)->index;
    fprintf(ctx->stream, "%s", &ctx->tir.global->strtab.ptr[name]);
    gen_type_after(ctx, type);
    fprintf(ctx->stream, ";\n");
}

static void gen_extern_function(GenContext *ctx, ValueId value) {
    TypeId type = get_value_type(ctx->tir, value);
    TypeId ret_type = get_function_type(ctx->tir, type).ret;

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

    int32_t name = get_value_data(ctx->tir, value)->index;
    fprintf(ctx->stream, "%s", &ctx->tir.global->strtab.ptr[name]);
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

static void gen_function_decl(GenContext *ctx, ValueId value, bool is_main) {
    if (is_main) {
        fprintf(ctx->stream, "int main(void);\n");
        return;
    }

    TypeId type = get_value_type(ctx->tir, value);
    TypeId ret_type = get_function_type(ctx->tir, type).ret;
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

    int32_t name = get_value_data(ctx->tir, value)->index;
    fprintf(ctx->stream, "%s", &ctx->tir.global->strtab.ptr[name]);
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

static bool is_lvalue(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_DEREF:
        case MIR_INDEX:
        case MIR_SLICE_INDEX:
        case MIR_CONST_INDEX:
        case MIR_ACCESS: return true;

        case MIR_ADDRESS:
        case MIR_PARAM:
        case MIR_ALLOC:
        case MIR_ASSIGN:
        case MIR_NEW_SLICE:
        case MIR_INT:
        case MIR_FLOAT:
        case MIR_STRING:
        case MIR_NULL:
        case MIR_TIR_VALUE:
        case MIR_MINUS:
        case MIR_NOT:
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
        case MIR_ITOF:
        case MIR_ITRUNC:
        case MIR_SEXT:
        case MIR_ZEXT:
        case MIR_FTOI:
        case MIR_FTRUNC:
        case MIR_FEXT:
        case MIR_PTR_CAST:
        case MIR_CALL:
        case MIR_BR:
        case MIR_BR_IF:
        case MIR_BR_IF_NOT:
        case MIR_RET_VOID:
        case MIR_RET: return false;
    }
    return false;
}

static void gen_string(GenContext *ctx, char const *str) {
    fprintf(ctx->stream, "\"");
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] >= 32 && str[i + 4] <= 126) {
            fprintf(ctx->stream, "%c", str[i + 4]);
        } else {
            fprintf(ctx->stream, "\\x%02X", (int) (unsigned char) str[i + 4]);
        }
    }
    fprintf(ctx->stream, "\"");
}

static void gen_value(GenContext *ctx, ValueId value) {
    ValueData const *data = get_value_data(ctx->tir, value);

    switch (get_value_tag(ctx->tir, value)) {
        case VAL_ERROR: {
            abort();
        }
        case VAL_FUNCTION:
        case VAL_EXTERN_FUNCTION:
        case VAL_EXTERN_VAR: {
            fprintf(ctx->stream, "%s", &ctx->tir.global->strtab.ptr[data->index]);
            break;
        }
        case VAL_STRING: {
            gen_string(ctx, get_value_str(ctx->tir, value));
            break;
        }
        case VAL_CONST_INT: {
            fprintf(ctx->stream, "%ld", get_value_int(ctx->tir, value));
            break;
        }
        case VAL_CONST_FLOAT: {
            fprintf(ctx->stream, "%f", get_value_float(ctx->tir, value));
            break;
        }
        case VAL_CONST_NULL: {
            fprintf(ctx->stream, "0");
            break;
        }
        case VAL_VARIABLE:
        case VAL_MUTABLE_VARIABLE:
        case VAL_TEMPORARY: {
            abort();
        }
    }
}

static void introduce_temporary(GenContext *ctx, MirId mir_id, TypeId type, bool is_pointer) {
    fprintf(ctx->stream, "    ");

    if (is_pointer) {
        gen_ptr_type_before(ctx, type);
    } else {
        gen_type_before(ctx, type);
    }

    fprintf(ctx->stream, "t%d", mir_id.private_field_id - ctx->mir_start);

    if (is_pointer) {
        gen_ptr_type_after(ctx, type);
    } else {
        gen_type_after(ctx, type);
    }

    fprintf(ctx->stream, " = ");
}

static void gen_operand(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_INT: {
            fprintf(ctx->stream, "%ld", get_mir_int(ctx->mir, mir_id));
            return;
        }
        case MIR_STRING: {
            ValueId value = get_mir_tir_value(ctx->mir, mir_id);
            gen_string(ctx, get_value_str(ctx->tir, value));
            return;
        }
        case MIR_TIR_VALUE: {
            ValueId operand = get_mir_tir_value(ctx->mir, mir_id);
            gen_value(ctx, operand);
            return;
        }
        case MIR_DEREF: {
            MirId operand = get_mir_unary(ctx->mir, mir_id);
            fprintf(ctx->stream, "(*t%d)", operand.private_field_id - ctx->mir_start);
            return;
        }
        default: {
            break;
        }
    }

    if (is_lvalue(ctx, mir_id)) {
        fprintf(ctx->stream, "(*t%d)", mir_id.private_field_id - ctx->mir_start);
    } else {
        fprintf(ctx->stream, "t%d", mir_id.private_field_id - ctx->mir_start);
    }
}

static void gen_alloc(GenContext *ctx, MirId mir_id) {
    TypeId local_type = get_mir_type(ctx->mir, mir_id);

    if (local_type.id == TYPE_VOID) {
        abort();
    }

    fprintf(ctx->stream, "    ");
    gen_type_before(ctx, local_type);
    fprintf(ctx->stream, "t%d", mir_id.private_field_id - ctx->mir_start);
    gen_type_after(ctx, local_type);
    fprintf(ctx->stream, ";\n");
}

static void gen_address(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, type, true);

    if (get_mir_tag(ctx->mir, operand) == MIR_STRING) {
        fputs("(", ctx->stream);
        gen_ptr_type_before(ctx, type);
        gen_ptr_type_after(ctx, type);
        fputs(") ", ctx->stream);
        gen_operand(ctx, operand);
        fprintf(ctx->stream, ";\n");
        return;
    }

    fputs("&", ctx->stream);
    gen_operand(ctx, operand);
    fprintf(ctx->stream, ";\n");
}

static void gen_unary(GenContext *ctx, MirId mir_id, char const *op) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, type, false);
    fputs(op, ctx->stream);
    gen_operand(ctx, operand);
    fprintf(ctx->stream, ";\n");
}

static void gen_binary(GenContext *ctx, MirId mir_id, char const *op) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, type, false);
    gen_operand(ctx, binary.left);
    fprintf(ctx->stream, " %s ", op);
    gen_operand(ctx, binary.right);
    fprintf(ctx->stream, ";\n");
}

static void gen_bool_binary(GenContext *ctx, MirId mir_id, char const *op) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, type_bool, false);
    gen_operand(ctx, binary.left);
    fprintf(ctx->stream, " %s ", op);
    gen_operand(ctx, binary.right);
    fprintf(ctx->stream, ";\n");
}

static void gen_mod(GenContext *ctx, MirId mir_id) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);

    if (type_is_int(type)) {
        gen_binary(ctx, mir_id, "%");
    } else {
        introduce_temporary(ctx, mir_id, type, false);
        fprintf(ctx->stream, "%s(", type.id == TYPE_f32 ? "__builtin_fmodf" : "__builtin_fmod");
        gen_operand(ctx, binary.left);
        fprintf(ctx->stream, ", ");
        gen_operand(ctx, binary.right);
        fprintf(ctx->stream, ");\n");
    }
}

static void gen_assign(GenContext *ctx, MirId mir_id) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);

    if (get_type_tag(ctx->tir, type) == TYPE_ARRAY) {
        fprintf(ctx->stream, "    __builtin_memcpy(&");
        gen_operand(ctx, binary.left);
        fprintf(ctx->stream, ", &");
        gen_operand(ctx, binary.right);
        fprintf(ctx->stream, ", %ld);\n", sizeof_type(ctx->tir, type, ctx->target));
    } else {
        fprintf(ctx->stream, "    ");
        gen_operand(ctx, binary.left);
        fprintf(ctx->stream, " = ");
        gen_operand(ctx, binary.right);
        fprintf(ctx->stream, ";\n");
    }
}

static void gen_new_slice(GenContext *ctx, MirId mir_id) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, type, false);
    fprintf(ctx->stream, "{");
    gen_operand(ctx, binary.left);
    fprintf(ctx->stream, ", (char *) t%d", binary.right.private_field_id - ctx->mir_start);
    fprintf(ctx->stream, "};\n");
}

static void gen_cast(GenContext *ctx, MirId mir_id) {
    MirAccess cast = get_mir_access(ctx->mir, mir_id);
    TypeId cast_type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, cast_type, false);
    fprintf(ctx->stream, "(");
    gen_type_before(ctx, cast_type);
    gen_type_after(ctx, cast_type);
    fprintf(ctx->stream, ") ");
    gen_operand(ctx, cast.operand);
    fprintf(ctx->stream, ";\n");
}

static void gen_zext(GenContext *ctx, MirId mir_id) {
    MirAccess cast = get_mir_access(ctx->mir, mir_id);
    TypeId cast_type = get_mir_type(ctx->mir, mir_id);
    TypeId operand_type = {cast.index};
    introduce_temporary(ctx, mir_id, cast_type, false);
    fprintf(ctx->stream, "(");
    gen_type_before(ctx, cast_type);
    gen_type_after(ctx, cast_type);
    fprintf(ctx->stream, ") ");
    gen_operand(ctx, cast.operand);

    int64_t int_size = sizeof_type(ctx->tir, operand_type, ctx->target);
    uint64_t mask = ((uint64_t) 1 << (int_size * 8)) - 1;
    fprintf(ctx->stream, " & 0x%lX;\n", mask);
}

static void gen_call(GenContext *ctx, MirId mir_id) {
    MirAccess call = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    FunctionType function_type = get_function_type(ctx->tir, type);
    int32_t arg_count = function_type.param_count;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_type_passed_by_ptr(ctx, function_type.ret);

    if (function_type.ret.id != TYPE_VOID) {
        if (implicit_return) {
            fprintf(ctx->stream, "    ");
            gen_type_before(ctx, function_type.ret);
            fprintf(ctx->stream, "t%d", mir_id.private_field_id - ctx->mir_start);
            gen_type_after(ctx, function_type.ret);
            fprintf(ctx->stream, ";\n");
            fprintf(ctx->stream, "    ");
        } else {
            introduce_temporary(ctx, mir_id, function_type.ret, false);
        }
    } else {
        fprintf(ctx->stream, "    ");
    }

    gen_operand(ctx, call.operand);
    fprintf(ctx->stream, "(");

    if (implicit_return) {
        fprintf(ctx->stream, "&");
        gen_operand(ctx, mir_id);

        if (arg_count) {
            fprintf(ctx->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }

        MirId arg = {get_mir_extra(ctx->mir, call.index + i)};
        gen_operand(ctx, arg);
    }

    fprintf(ctx->stream, ");\n");
}

static void gen_index(GenContext *ctx, MirId mir_id) {
    MirBinary index = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, remove_c_pointer_like(ctx->tir, type), true);
    fprintf(ctx->stream, "&");
    gen_operand(ctx, index.left);
    fprintf(ctx->stream, "[");
    gen_operand(ctx, index.right);
    fprintf(ctx->stream, "];\n");
}

static void gen_slice_index(GenContext *ctx, MirId mir_id) {
    MirBinary index = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    TypeId elem_type = remove_slice(ctx->tir, type);
    introduce_temporary(ctx, mir_id, elem_type, true);
    fprintf(ctx->stream, "&((");
    gen_ptr_type_before(ctx, elem_type);
    gen_ptr_type_after(ctx, elem_type);
    fprintf(ctx->stream, ")");
    gen_operand(ctx, index.left);
    fprintf(ctx->stream, "._1)[");
    gen_operand(ctx, index.right);
    fprintf(ctx->stream, "];\n");
}

static void gen_const_index(GenContext *ctx, MirId mir_id) {
    MirAccess index = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    introduce_temporary(ctx, mir_id, remove_c_pointer_like(ctx->tir, type), true);
    fprintf(ctx->stream, "&");
    gen_operand(ctx, index.operand);
    fprintf(ctx->stream, "[%d];\n", index.index);
}

static void gen_access(GenContext *ctx, MirId mir_id) {
    MirAccess access = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    TypeId field_type = get_any_struct_type_field(ctx->tir, type, access.index);
    introduce_temporary(ctx, mir_id, field_type, true);
    fprintf(ctx->stream, "&");
    gen_operand(ctx, access.operand);
    fprintf(ctx->stream, "._%d;\n", access.index);
}

static void gen_br(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    fprintf(ctx->stream, "    goto L%d;\n", br.index);
}

static void gen_br_if(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    fprintf(ctx->stream, "    if (");
    gen_operand(ctx, br.operand);
    fprintf(ctx->stream, ") goto L%d;\n", br.index);
}

static void gen_br_if_not(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    fprintf(ctx->stream, "    if (!");
    gen_operand(ctx, br.operand);
    fprintf(ctx->stream, ") goto L%d;\n", br.index);
}

static void gen_ret_void(GenContext *ctx) {
    if (ctx->is_main) {
        fprintf(ctx->stream, "    return 0;\n");
    } else {
        fprintf(ctx->stream, "    return;\n");
    }
}

static void gen_ret(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);

    if (!is_type_passed_by_ptr(ctx, ctx->return_type)) {
        fprintf(ctx->stream, "    return ");
        gen_operand(ctx, operand);
        fprintf(ctx->stream, ";\n");
    } else {
        fprintf(ctx->stream, "    __builtin_memcpy(ret, &");
        gen_operand(ctx, operand);
        fprintf(ctx->stream, ", %ld);\n", sizeof_type(ctx->tir, ctx->return_type, ctx->target));
        fprintf(ctx->stream, "    return ret;\n");
    }
}

static void gen_instruction(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_PARAM: break;
        case MIR_ALLOC: gen_alloc(ctx, mir_id); break;
        case MIR_INT: break;
        case MIR_FLOAT: break;
        case MIR_STRING: break;
        case MIR_NULL: break;
        case MIR_TIR_VALUE: break;
        case MIR_DEREF: break;
        case MIR_ASSIGN: gen_assign(ctx, mir_id); break;
        case MIR_NEW_SLICE: gen_new_slice(ctx, mir_id); break;
        case MIR_MINUS: gen_unary(ctx, mir_id, "-"); break;
        case MIR_NOT: gen_unary(ctx, mir_id, "!"); break;
        case MIR_ADDRESS: gen_address(ctx, mir_id); break;
        case MIR_ADD: gen_binary(ctx, mir_id, "+"); break;
        case MIR_SUB: gen_binary(ctx, mir_id, "-"); break;
        case MIR_MUL: gen_binary(ctx, mir_id, "*"); break;
        case MIR_DIV: gen_binary(ctx, mir_id, "/"); break;
        case MIR_MOD: gen_mod(ctx, mir_id); break;
        case MIR_AND: gen_binary(ctx, mir_id, "&"); break;
        case MIR_OR: gen_binary(ctx, mir_id, "|"); break;
        case MIR_XOR: gen_binary(ctx, mir_id, "^"); break;
        case MIR_SHL: gen_binary(ctx, mir_id, "<<"); break;
        case MIR_SHR: gen_binary(ctx, mir_id, ">>"); break;
        case MIR_EQ: gen_bool_binary(ctx, mir_id, "=="); break;
        case MIR_NE: gen_bool_binary(ctx, mir_id, "!="); break;
        case MIR_LT: gen_bool_binary(ctx, mir_id, "<"); break;
        case MIR_GT: gen_bool_binary(ctx, mir_id, ">"); break;
        case MIR_LE: gen_bool_binary(ctx, mir_id, "<="); break;
        case MIR_GE: gen_bool_binary(ctx, mir_id, ">="); break;

        case MIR_ITOF:
        case MIR_ITRUNC:
        case MIR_SEXT:
        case MIR_FTOI:
        case MIR_FTRUNC:
        case MIR_FEXT:
        case MIR_PTR_CAST: gen_cast(ctx, mir_id); break;

        case MIR_ZEXT: gen_zext(ctx, mir_id); break;
        case MIR_CALL: gen_call(ctx, mir_id); break;
        case MIR_INDEX: gen_index(ctx, mir_id); break;
        case MIR_SLICE_INDEX: gen_slice_index(ctx, mir_id); break;
        case MIR_CONST_INDEX: gen_const_index(ctx, mir_id); break;
        case MIR_ACCESS: gen_access(ctx, mir_id); break;
        case MIR_BR: gen_br(ctx, mir_id); break;
        case MIR_BR_IF: gen_br_if(ctx, mir_id); break;
        case MIR_BR_IF_NOT: gen_br_if_not(ctx, mir_id); break;
        case MIR_RET_VOID: gen_ret_void(ctx); break;
        case MIR_RET: gen_ret(ctx, mir_id); break;
    }
}

static void gen_function(GenContext *ctx, int32_t mir_start, int32_t mir_end, ValueId value, bool is_main) {
    ctx->mir_start = mir_start;
    TypeId type = get_value_type(ctx->tir, value);
    TypeId ret_type = get_function_type(ctx->tir, type).ret;
    ctx->return_type = ret_type;
    ctx->is_main = is_main;

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

        int32_t name = get_value_data(ctx->tir, value)->index;
        fprintf(ctx->stream, "%s", &ctx->tir.global->strtab.ptr[name]);
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
        MirId mir_id = {i};

        if (i != mir_start && is_mir_terminator(get_mir_tag(ctx->mir, (MirId) {i - 1}))) {
            fprintf(ctx->stream, "L%d:\n    ;\n", blocks++);
        }

        gen_instruction(ctx, mir_id);
    }

    fprintf(ctx->stream, "}\n");
}

static void gen_struct_decl(GenContext *ctx, TypeId type) {
    fprintf(ctx->stream, "struct _S%s;\n", ctx->tir.global->strtab.ptr + get_struct_type(ctx->tir, type).name);
}

static void gen_struct(GenContext *ctx, TypeId type) {
    StructType s = get_struct_type(ctx->tir, type);
    fprintf(ctx->stream, "struct _S%s {\n", ctx->tir.global->strtab.ptr + s.name);

    for (int32_t i = 0; i < s.field_count; i++) {
        TypeId field_type = get_struct_type_field(ctx->tir, type, i);
        fprintf(ctx->stream, "    ");
        gen_type_before(ctx, field_type);
        fprintf(ctx->stream, "_%d", i);
        gen_type_after(ctx, field_type);
        fprintf(ctx->stream, ";\n");
    }

    fprintf(ctx->stream, "};\n");
}

void gen_c(GenInput *input, Target target) {
    FILE *stream = fopen("a.c", "w");

    if (!stream) {
        fprintf(stderr, "failed to write to file\n");
        exit(-1);
    }

    fprintf(stream, "#include <stdint.h>\n\n");
    int ptr_bits = sizeof_pointer(target) * 8;
    fprintf(stream, "struct Slice { int%d_t _0; char *_1; };\n\n", ptr_bits);

    GenContext ctx = {
        .target = target,
        .tir = {
            .global = &input->global_deps,
            .thread = NULL,
        },
        .mir = &input->mir_result->mir,
        .stream = stream,
    };

    for (int32_t i = 0; i < input->declarations.structs.len; i++) {
        TypeId type = input->declarations.structs.ptr[i];
        gen_struct_decl(&ctx, type);
    }

    for (int32_t i = 0; i < input->declarations.structs.len; i++) {
        TypeId type = input->declarations.structs.ptr[i];
        gen_struct(&ctx, type);
    }

    for (int32_t i = 0; i < input->declarations.extern_vars.len; i++) {
        ValueId value = input->declarations.extern_vars.ptr[i];
        gen_extern_var(&ctx, value);
    }

    for (int32_t i = 0; i < input->declarations.extern_functions.len; i++) {
        ValueId value = input->declarations.extern_functions.ptr[i];
        gen_extern_function(&ctx, value);
    }

    for (int32_t i = 0; i < input->declarations.functions.len; i++) {
        ValueId value = input->declarations.functions.ptr[i];
        gen_function_decl(&ctx, value, input->declarations.main.id == value.id);
    }

    for (int32_t i = 0; i < input->declarations.functions.len; i++) {
        ctx.tir.thread = &input->insts[i];
        ValueId value = input->declarations.functions.ptr[i];
        gen_function(&ctx, input->mir_result->ends[i], input->mir_result->ends[i + 1], value, input->declarations.main.id == value.id);
    }

    fclose(stream);
}
