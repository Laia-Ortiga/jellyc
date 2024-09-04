#include "gen.h"

#include "adt.h"
#include "arena.h"
#include "data/mir.h"
#include "data/tir.h"
#include "fwd.h"

#include <stdlib.h>

typedef struct {
    Mir *mir;
    int32_t mir_start;
    Arena scratch;
    int32_t *temporaries;
    Vec(char const *) strings;
    int32_t tmp_count;
    int32_t blocks;
    TypeId return_type;
    bool is_main;
    Target target;
    TirContext tir;
    FILE *stream;
} GenContext;

static void gen_type(GenContext *ctx, TypeId type) {
    switch (get_type_tag(ctx->tir, type)) {
        case TYPE_PRIMITIVE: {
            switch ((PrimitiveType) type.id) {
                case TYPE_INVALID:
                case TYPE_COUNT: break;

                case TYPE_VOID: fprintf(ctx->stream, "void"); return;

                case TYPE_i8:
                case TYPE_char:
                case TYPE_byte: fprintf(ctx->stream, "i8"); return;

                case TYPE_i16: fprintf(ctx->stream, "i16"); return;
                case TYPE_i32: fprintf(ctx->stream, "i32"); return;
                case TYPE_i64: fprintf(ctx->stream, "i64"); return;

                case TYPE_SIZE_TAG:
                case TYPE_ALIGNMENT_TAG:
                case TYPE_isize: fprintf(ctx->stream, "i%d", sizeof_pointer(ctx->target) * 8); return;

                case TYPE_f32: fprintf(ctx->stream, "float"); return;
                case TYPE_f64: fprintf(ctx->stream, "double"); return;
                case TYPE_bool: fprintf(ctx->stream, "i1"); return;
            }
        }
        case TYPE_TYPE_PARAMETER: {
            break;
        }
        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx->tir, type);
            int64_t length = get_array_length_type(ctx->tir, array.index);
            fprintf(ctx->stream, "[%ld x ", length);
            gen_type(ctx, array.elem);
            fprintf(ctx->stream, "]");
            return;
        }
        case TYPE_ARRAY_LENGTH: {
            fprintf(ctx->stream, "i%d", sizeof_pointer(ctx->target) * 8);
            return;
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_FUNCTION: {
            fprintf(ctx->stream, "ptr");
            return;
        }
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: {
            fprintf(ctx->stream, "%%slice");
            return;
        }
        case TYPE_STRUCT: {
            fprintf(ctx->stream, "%%_%s", ctx->tir.global->strtab.ptr + get_struct_type(ctx->tir, type).name);
            return;
        }
        case TYPE_ENUM: {
            gen_type(ctx, get_enum_type(ctx->tir, type).repr);
            return;
        }
        case TYPE_NEWTYPE: {
            gen_type(ctx, get_newtype_type(ctx->tir, type).type);
            return;
        }
        case TYPE_TAGGED: {
            gen_type(ctx, get_tagged_type(ctx->tir, type).inner);
            return;
        }
        case TYPE_LINEAR: {
            gen_type(ctx, get_linear_elem_type(ctx->tir, type));
            return;
        }
    }
    abort();
}

static void gen_params(GenContext *ctx, TypeId type) {
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

        TypeId param_type = get_function_type_param(ctx->tir, type, i);
        if (is_aggregate_type(ctx->tir, param_type)) {
            fprintf(ctx->stream, "ptr");
        } else {
            gen_type(ctx, param_type);
        }
    }

    fprintf(ctx->stream, ")");
}

static void gen_ret_type(GenContext *ctx, TypeId type) {
    if (is_aggregate_type(ctx->tir, type)) {
        fprintf(ctx->stream, "ptr");
        return;
    }
    gen_type(ctx, type);
}

static void gen_extern_var(GenContext *ctx, ValueId value) {
    TypeId type = get_value_type(ctx->tir, value);
    int32_t name = get_value_data(ctx->tir, value)->index;
    fprintf(ctx->stream, "@%s = external global ", &ctx->tir.global->strtab.ptr[name]);
    gen_type(ctx, type);
    fprintf(ctx->stream, ", align %d\n", alignof_type(ctx->tir, type, ctx->target));
}

static void gen_extern_function(GenContext *ctx, ValueId value) {
    TypeId type = get_value_type(ctx->tir, value);
    TypeId ret_type = get_function_type(ctx->tir, type).ret;
    fprintf(ctx->stream, "declare ");
    gen_ret_type(ctx, ret_type);
    int32_t name = get_value_data(ctx->tir, value)->index;
    fprintf(ctx->stream, " @%s", &ctx->tir.global->strtab.ptr[name]);
    gen_params(ctx, type);
    fprintf(ctx->stream, "\n");
}

static bool is_lvalue(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_PARAM: {
            return is_aggregate_type(ctx->tir, get_mir_type(ctx->mir, mir_id));
        }
        case MIR_CALL: {
            FunctionType func_type = get_function_type(ctx->tir, get_mir_type(ctx->mir, mir_id));
            return is_aggregate_type(ctx->tir, func_type.ret);
        }
        case MIR_TIR_VALUE: {
            ValueId value = get_mir_tir_value(ctx->mir, mir_id);
            switch (get_value_tag(ctx->tir, value)) {
                case VAL_EXTERN_VAR: {
                    return true;
                }
                case VAL_FUNCTION:
                case VAL_EXTERN_FUNCTION:
                case VAL_STRING:
                case VAL_CONST_INT:
                case VAL_CONST_FLOAT:
                case VAL_CONST_NULL: {
                    return false;
                }
                default: {
                    abort();
                }
            }
        }
        case MIR_ALLOC:
        case MIR_DEREF:
        case MIR_INDEX:
        case MIR_SLICE_INDEX:
        case MIR_CONST_INDEX:
        case MIR_ACCESS:
        case MIR_NEW_SLICE: {
            return true;
        }
        case MIR_ADDRESS:
        case MIR_ASSIGN:
        case MIR_INT:
        case MIR_FLOAT:
        case MIR_STRING:
        case MIR_NULL:
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
        case MIR_BR:
        case MIR_BR_IF:
        case MIR_BR_IF_NOT:
        case MIR_RET_VOID:
        case MIR_RET: {
            return false;
        }
    }
    return false;
}

static void gen_string(GenContext *ctx, int32_t index, char const *str) {
    uint64_t len = (uint64_t) str[0];
    len |= (uint64_t) str[1] << 8;
    len |= (uint64_t) str[2] << 16;
    len |= (uint64_t) str[3] << 24;
    fprintf(ctx->stream, "@s%d = private unnamed_addr constant [%lu x i8] c\"", index, len + 1);
    for (uint64_t i = 0; i < len; i++) {
        if (str[i + 4] >= 32 && str[i + 4] <= 126) {
            fprintf(ctx->stream, "%c", str[i + 4]);
        } else {
            fprintf(ctx->stream, "\\%02X", (int) (unsigned char) str[i + 4]);
        }
    }
    fprintf(ctx->stream, "\\00\", align 1\n");
}

static void gen_value(GenContext *ctx, ValueId value) {
    switch (get_value_tag(ctx->tir, value)) {
        case VAL_ERROR:
        case VAL_VARIABLE:
        case VAL_MUTABLE_VARIABLE:
        case VAL_TEMPORARY: {
            abort();
        }
        case VAL_FUNCTION:
        case VAL_EXTERN_FUNCTION:
        case VAL_EXTERN_VAR: {
            ValueData const *data = get_value_data(ctx->tir, value);
            fprintf(ctx->stream, "@%s", &ctx->tir.global->strtab.ptr[data->index]);
            break;
        }
        case VAL_STRING: {
            fprintf(ctx->stream, "@s%d", ctx->strings.len);
            vec_push(&ctx->strings, get_value_str(ctx->tir, value));
            break;
        }
        case VAL_CONST_INT: {
            fprintf(ctx->stream, "%ld", get_value_int(ctx->tir, value));
            break;
        }
        case VAL_CONST_FLOAT: {
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
        case VAL_CONST_NULL: {
            fprintf(ctx->stream, "null");
            break;
        }
    }
}

static int32_t new_tmp(GenContext *ctx, MirId mir_id) {
    ctx->temporaries[mir_id.private_field_id - ctx->mir_start] = ctx->tmp_count;
    return ctx->tmp_count++;
}

static void gen_operand_address(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_INT: {
            fprintf(ctx->stream, "%ld", get_mir_int(ctx->mir, mir_id));
            break;
        }
        case MIR_STRING: {
            ValueId value = get_mir_tir_value(ctx->mir, mir_id);
            fprintf(ctx->stream, "@s%d", ctx->strings.len);
            vec_push(&ctx->strings, get_value_str(ctx->tir, value));
            break;
        }
        case MIR_TIR_VALUE: {
            ValueId operand = get_mir_tir_value(ctx->mir, mir_id);
            gen_value(ctx, operand);
            break;
        }
        case MIR_ADDRESS: {
            MirId operand = get_mir_unary(ctx->mir, mir_id);
            gen_operand_address(ctx, operand);
            ctx->temporaries[mir_id.private_field_id - ctx->mir_start] = ctx->temporaries[operand.private_field_id - ctx->mir_start];
            break;
        }
        default: {
            fprintf(ctx->stream, "%%%d", ctx->temporaries[mir_id.private_field_id - ctx->mir_start]);
            break;
        }
    }
}

static int32_t load_operand(GenContext *ctx, MirId mir_id, TypeId type) {
    if (is_lvalue(ctx, mir_id)) {
        int32_t tmp = ctx->tmp_count++;
        fprintf(ctx->stream, "  %%%d = load ", tmp);
        gen_type(ctx, type);
        fprintf(ctx->stream, ", ptr ");
        gen_operand_address(ctx, mir_id);
        fprintf(ctx->stream, "\n");
        return tmp;
    }
    return -1;
}

static void gen_operand(GenContext *ctx, MirId mir_id, int32_t place) {
    if (is_lvalue(ctx, mir_id)) {
        fprintf(ctx->stream, "%%%d", place);
    } else {
        gen_operand_address(ctx, mir_id);
    }
}

static void gen_alloc(GenContext *ctx, MirId mir_id) {
    TypeId local_type = get_mir_type(ctx->mir, mir_id);
    if (local_type.id == TYPE_VOID) {
        return;
    }
    fprintf(ctx->stream, "  %%%d = alloca ", new_tmp(ctx, mir_id));
    gen_type(ctx, local_type);
    fprintf(ctx->stream, "\n");
}

static void gen_call_alloc(GenContext *ctx, MirId mir_id) {
    TypeId type = get_mir_type(ctx->mir, mir_id);
    FunctionType function_type = get_function_type(ctx->tir, type);
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(ctx->tir, function_type.ret);
    if (function_type.ret.id != TYPE_VOID && implicit_return) {
        fprintf(ctx->stream, "  %%%d = alloca ", new_tmp(ctx, mir_id));
        gen_type(ctx, function_type.ret);
        fprintf(ctx->stream, "\n");
    }
}

static void gen_new_slice_alloc(GenContext *ctx, MirId mir_id) {
    fprintf(ctx->stream, "  %%%d = alloca %%slice\n", new_tmp(ctx, mir_id));
}

static void gen_negative(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, operand, type);
    char const *op = "sub";
    char const *zero = "0";
    if (type_is_float(type)) {
        op = "fsub";
        zero = "0.0";
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, mir_id), op);
    gen_type(ctx, type);
    fprintf(ctx->stream, " %s, ", zero);
    gen_operand(ctx, operand, llvm_operand);
    fprintf(ctx->stream, "\n");
}

static void gen_not(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, operand, type);
    fprintf(ctx->stream, "  %%%d = xor ", new_tmp(ctx, mir_id));
    gen_type(ctx, type);
    fprintf(ctx->stream, " -1, ");
    gen_operand(ctx, operand, llvm_operand);
    fprintf(ctx->stream, "\n");
}

static void gen_binary(GenContext *ctx, MirId mir_id, char const *op) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_left = load_operand(ctx, binary.left, type);
    int32_t llvm_right = load_operand(ctx, binary.right, type);
    fprintf(ctx->stream, "  %%t%d = %s ", mir_id.private_field_id, op);
    gen_type(ctx, type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, binary.left, llvm_left);
    fprintf(ctx->stream, ", ");
    gen_operand(ctx, binary.right, llvm_right);
    fprintf(ctx->stream, "\n");
}

static void gen_overloaded_binary(GenContext *ctx, MirId mir_id, char const *op, char const *float_op) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_left = load_operand(ctx, binary.left, type);
    int32_t llvm_right = load_operand(ctx, binary.right, type);
    if (type_is_float(type)) {
        op = float_op;
    }
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, mir_id), op);
    gen_type(ctx, type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, binary.left, llvm_left);
    fprintf(ctx->stream, ", ");
    gen_operand(ctx, binary.right, llvm_right);
    fprintf(ctx->stream, "\n");
}

static bool should_deref(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_ALLOC: {
            return true;
        }
        case MIR_PARAM: {
            return is_aggregate_type(ctx->tir, get_mir_type(ctx->mir, mir_id));
        }
        case MIR_CALL: {
            FunctionType func_type = get_function_type(ctx->tir, get_mir_type(ctx->mir, mir_id));
            return is_aggregate_type(ctx->tir, func_type.ret);
        }
        default: {
            return false;
        }
    }
}

static void gen_deref(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    if (!should_deref(ctx, operand)) {
        ctx->temporaries[mir_id.private_field_id - ctx->mir_start] = ctx->temporaries[operand.private_field_id - ctx->mir_start];
        return;
    }
    int32_t tmp = new_tmp(ctx, mir_id);
    fprintf(ctx->stream, "  %%%d = load ptr, ptr %%%d\n", tmp, ctx->temporaries[operand.private_field_id - ctx->mir_start]);
}

static void gen_assign(GenContext *ctx, MirId mir_id) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_right = load_operand(ctx, binary.right, type);
    fprintf(ctx->stream, "  store ");
    gen_type(ctx, type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, binary.right, llvm_right);
    fprintf(ctx->stream, ", ptr ");
    gen_operand_address(ctx, binary.left);
    fprintf(ctx->stream, "\n");
}

static void gen_new_slice(GenContext *ctx, MirId mir_id) {
    MirBinary binary = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);

    int32_t length_address = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds %%slice, ptr ", length_address);
    gen_operand_address(ctx, mir_id);
    fprintf(ctx->stream, ", i64 0, i32 0\n");

    int32_t llvm_left = load_operand(ctx, binary.left, type);
    fprintf(ctx->stream, "  store ");
    gen_type(ctx, type_isize);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, binary.left, llvm_left);
    fprintf(ctx->stream, ", ptr %%%d\n", length_address);

    int32_t data_address = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds %%slice, ptr ", data_address);
    gen_operand_address(ctx, mir_id);
    fprintf(ctx->stream, ", i64 0, i32 1\n");

    fprintf(ctx->stream, "  store ptr ");
    gen_operand_address(ctx, binary.right);
    fprintf(ctx->stream, ", ptr %%%d\n", data_address);
}

static void gen_cast(GenContext *ctx, MirId mir_id, char const *op) {
    MirAccess cast = get_mir_access(ctx->mir, mir_id);
    TypeId type = {cast.index};
    TypeId cast_type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, cast.operand, type);
    fprintf(ctx->stream, "  %%%d = %s ", new_tmp(ctx, mir_id), op);
    gen_type(ctx, type);
    fprintf(ctx->stream, " ");
    gen_operand(ctx, cast.operand, llvm_operand);
    fprintf(ctx->stream, " to ");
    gen_type(ctx, cast_type);
    fprintf(ctx->stream, "\n");
}

static void gen_call(GenContext *ctx, MirId mir_id) {
    MirAccess call = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    FunctionType function_type = get_function_type(ctx->tir, type);
    int32_t arg_count = function_type.param_count;
    bool implicit_return = function_type.ret.id != TYPE_VOID && is_aggregate_type(ctx->tir, function_type.ret);

    int32_t llvm_callee = load_operand(ctx, call.operand, type);
    int32_t *llvm_args = arena_alloc(&ctx->scratch, int32_t, arg_count);
    for (int32_t i = 0; i < arg_count; i++) {
        MirId arg = {get_mir_extra(ctx->mir, call.index + i)};
        if (is_aggregate_type(ctx->tir, get_function_type_param(ctx->tir, type, i))) {
            llvm_args[i] = -1;
        } else {
            llvm_args[i] = load_operand(ctx, arg, get_function_type_param(ctx->tir, type, i));
        }
    }

    fprintf(ctx->stream, "  ");
    if (function_type.ret.id != TYPE_VOID) {
        if (is_aggregate_type(ctx->tir, function_type.ret)) {
            fprintf(ctx->stream, "%%%d = ", ctx->tmp_count++);
        } else {
            fprintf(ctx->stream, "%%%d = ", new_tmp(ctx, mir_id));
        }
    }
    fprintf(ctx->stream, "call ");
    if (implicit_return) {
        fprintf(ctx->stream, "ptr");
    } else {
        gen_type(ctx, function_type.ret);
    }
    fprintf(ctx->stream, " ");
    gen_operand(ctx, call.operand, llvm_callee);
    fprintf(ctx->stream, "(");

    if (implicit_return) {
        fprintf(ctx->stream, "ptr ");
        gen_operand_address(ctx, mir_id);
        if (arg_count) {
            fprintf(ctx->stream, ", ");
        }
    }

    for (int32_t i = 0; i < arg_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }

        MirId arg = {get_mir_extra(ctx->mir, call.index + i)};
        if (is_aggregate_type(ctx->tir, get_function_type_param(ctx->tir, type, i))) {
            fprintf(ctx->stream, "ptr");
            fprintf(ctx->stream, " ");
            gen_operand_address(ctx, arg);
        } else {
            gen_type(ctx, get_function_type_param(ctx->tir, type, i));
            fprintf(ctx->stream, " ");
            gen_operand(ctx, arg, llvm_args[i]);
        }
    }

    fprintf(ctx->stream, ")\n");
}

static void gen_index(GenContext *ctx, MirId mir_id) {
    MirBinary index = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_right = load_operand(ctx, index.right, type_isize);
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", new_tmp(ctx, mir_id));
    gen_type(ctx, type);
    fprintf(ctx->stream, ", ptr ");
    gen_operand_address(ctx, index.left);
    fprintf(ctx->stream, ", i64 0, i64 ");
    gen_operand(ctx, index.right, llvm_right);
    fprintf(ctx->stream, "\n");
}

static void gen_slice_index(GenContext *ctx, MirId mir_id) {
    MirBinary index = get_mir_binary(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    int32_t llvm_right = load_operand(ctx, index.right, type_isize);
    int32_t data_address = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", data_address);
    gen_type(ctx, type);
    fprintf(ctx->stream, ", ptr ");
    gen_operand_address(ctx, index.left);
    fprintf(ctx->stream, ", i64 0, i32 1\n");

    int32_t data = ctx->tmp_count++;
    fprintf(ctx->stream, "  %%%d = load ptr, ptr %%%d\n", data, data_address);

    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", new_tmp(ctx, mir_id));
    gen_type(ctx, remove_slice(ctx->tir, type));
    fprintf(ctx->stream, ", ptr %%%d, i64 ", data);
    gen_operand(ctx, index.right, llvm_right);
    fprintf(ctx->stream, "\n");
}

static void gen_const_index(GenContext *ctx, MirId mir_id) {
    MirAccess index = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", new_tmp(ctx, mir_id));
    gen_type(ctx, type);
    fprintf(ctx->stream, ", ptr ");
    gen_operand_address(ctx, index.operand);
    fprintf(ctx->stream, ", i64 0, i64 %d\n", index.index);
}

static void gen_access(GenContext *ctx, MirId mir_id) {
    MirAccess access = get_mir_access(ctx->mir, mir_id);
    TypeId type = get_mir_type(ctx->mir, mir_id);
    fprintf(ctx->stream, "  %%%d = getelementptr inbounds ", new_tmp(ctx, mir_id));
    gen_type(ctx, type);
    fprintf(ctx->stream, ", ptr ");
    gen_operand_address(ctx, access.operand);
    fprintf(ctx->stream, ", i64 0, i32 %d\n", access.index);
}

static void gen_br(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    fprintf(ctx->stream, "  br label %%L.%d\n", br.index);
}

static void gen_br_if(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, br.operand, type_bool);
    fprintf(ctx->stream, "  br i1 ");
    gen_operand(ctx, br.operand, llvm_operand);
    fprintf(ctx->stream, ", label %%L.%d, label %%L.%d\n", br.index, ctx->blocks);
}

static void gen_br_if_not(GenContext *ctx, MirId mir_id) {
    MirAccess br = get_mir_access(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, br.operand, type_bool);
    fprintf(ctx->stream, "  br i1 ");
    gen_operand(ctx, br.operand, llvm_operand);
    fprintf(ctx->stream, ", label %%L.%d, label %%L.%d\n", ctx->blocks, br.index);
}

static void gen_ret_void(GenContext *ctx) {
    if (ctx->is_main) {
        fprintf(ctx->stream, "  ret i32 0\n");
    } else {
        fprintf(ctx->stream, "  ret void\n");
    }
}

static void gen_ret(GenContext *ctx, MirId mir_id) {
    MirId operand = get_mir_unary(ctx->mir, mir_id);
    int32_t llvm_operand = load_operand(ctx, operand, ctx->return_type);

    if (!is_aggregate_type(ctx->tir, ctx->return_type)) {
        fprintf(ctx->stream, "  ret ");
        gen_type(ctx, ctx->return_type);
        fprintf(ctx->stream, " ");
        gen_operand(ctx, operand, llvm_operand);
        fprintf(ctx->stream, "\n");
    } else {
        fprintf(ctx->stream, "  store ");
        gen_type(ctx, ctx->return_type);
        fprintf(ctx->stream, " ");
        gen_operand(ctx, operand, llvm_operand);
        fprintf(ctx->stream, ", ptr %%0\n");

        fprintf(ctx->stream, "  ret ptr %%0\n");
    }
}

static void gen_instruction(GenContext *ctx, MirId mir_id) {
    switch (get_mir_tag(ctx->mir, mir_id)) {
        case MIR_PARAM: break;
        case MIR_ALLOC: break;
        case MIR_INT: break;
        case MIR_FLOAT: break;
        case MIR_STRING: break;
        case MIR_NULL: break;
        case MIR_TIR_VALUE: break;
        case MIR_ADDRESS: break;
        case MIR_DEREF: gen_deref(ctx, mir_id); break;
        case MIR_ASSIGN: gen_assign(ctx, mir_id); break;
        case MIR_NEW_SLICE: gen_new_slice(ctx, mir_id); break;
        case MIR_MINUS: gen_negative(ctx, mir_id); break;
        case MIR_NOT: gen_not(ctx, mir_id); break;
        case MIR_ADD: gen_overloaded_binary(ctx, mir_id, "add", "fadd"); break;
        case MIR_SUB: gen_overloaded_binary(ctx, mir_id, "sub", "fsub"); break;
        case MIR_MUL: gen_overloaded_binary(ctx, mir_id, "mul", "fmul"); break;
        case MIR_DIV: gen_overloaded_binary(ctx, mir_id, "sdiv", "fdiv"); break;
        case MIR_MOD: gen_overloaded_binary(ctx, mir_id, "srem", "frem"); break;
        case MIR_AND: gen_binary(ctx, mir_id, "and"); break;
        case MIR_OR: gen_binary(ctx, mir_id, "or"); break;
        case MIR_XOR: gen_binary(ctx, mir_id, "xor"); break;
        case MIR_SHL: gen_binary(ctx, mir_id, "shl"); break;
        case MIR_SHR: gen_binary(ctx, mir_id, "ashr"); break;
        case MIR_EQ: gen_overloaded_binary(ctx, mir_id, "icmp eq", "fcmp eq"); break;
        case MIR_NE: gen_overloaded_binary(ctx, mir_id, "icmp ne", "fcmp ne"); break;
        case MIR_LT: gen_overloaded_binary(ctx, mir_id, "icmp slt", "fcmp lt"); break;
        case MIR_GT: gen_overloaded_binary(ctx, mir_id, "icmp sgt", "fcmp gt"); break;
        case MIR_LE: gen_overloaded_binary(ctx, mir_id, "icmp sle", "fcmp le"); break;
        case MIR_GE: gen_overloaded_binary(ctx, mir_id, "icmp sge", "fcmp ge"); break;

        case MIR_ITOF: gen_cast(ctx, mir_id, "sitofp"); break;
        case MIR_ITRUNC: gen_cast(ctx, mir_id, "trunc"); break;
        case MIR_SEXT: gen_cast(ctx, mir_id, "sext"); break;
        case MIR_ZEXT: gen_cast(ctx, mir_id, "zext"); break;
        case MIR_FTOI: gen_cast(ctx, mir_id, "fptosi"); break;
        case MIR_FTRUNC: gen_cast(ctx, mir_id, "fptrunc"); break;
        case MIR_FEXT: gen_cast(ctx, mir_id, "fpext"); break;
        case MIR_PTR_CAST: gen_cast(ctx, mir_id, "bitcast"); break;

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
    ctx->temporaries = arena_alloc(&ctx->scratch, int32_t, mir_end - mir_start);
    ctx->tmp_count = 0;
    TypeId type = get_value_type(ctx->tir, value);
    TypeId ret_type = get_function_type(ctx->tir, type).ret;
    ctx->return_type = ret_type;
    ctx->is_main = is_main;
    if (is_main) {
        fprintf(ctx->stream, "define i32 @main");
    } else {
        fprintf(ctx->stream, "define private ");
        gen_ret_type(ctx, ret_type);
        int32_t name = get_value_data(ctx->tir, value)->index;
        fprintf(ctx->stream, " @%s", &ctx->tir.global->strtab.ptr[name]);
    }
    gen_params(ctx, type);
    fprintf(ctx->stream, " {\n");
    for (int32_t i = 0; i < get_function_type(ctx->tir, type).param_count; i++) {
        MirId mir_id = {i + mir_start};
        ctx->temporaries[i] = new_tmp(ctx, mir_id);
    }
    ctx->blocks = 1;
    ctx->tmp_count++;
    for (int32_t i = mir_start; i < mir_end; i++) {
        MirId mir_id = {i};
        switch (get_mir_tag(ctx->mir, (MirId) {i})) {
            case MIR_ALLOC: gen_alloc(ctx, mir_id); break;
            case MIR_CALL: gen_call_alloc(ctx, mir_id); break;
            case MIR_NEW_SLICE: gen_new_slice_alloc(ctx, mir_id); break;
            default: break;
        }
    }
    for (int32_t i = mir_start; i < mir_end; i++) {
        MirId mir_id = {i};
        if (i != mir_start && is_mir_terminator(get_mir_tag(ctx->mir, (MirId) {i - 1}))) {
            fprintf(ctx->stream, "L.%d:\n", ctx->blocks++);
        }
        gen_instruction(ctx, mir_id);
    }

    fprintf(ctx->stream, "}\n");
}

static void gen_struct(GenContext *ctx, TypeId type) {
    StructType s = get_struct_type(ctx->tir, type);
    fprintf(ctx->stream, "%%_%s = type { ", ctx->tir.global->strtab.ptr + s.name);

    for (int32_t i = 0; i < s.field_count; i++) {
        if (i != 0) {
            fprintf(ctx->stream, ", ");
        }
        TypeId field_type = get_struct_type_field(ctx->tir, type, i);
        gen_type(ctx, field_type);
    }

    fprintf(ctx->stream, " }\n");
}

void gen_llvm(GenInput *input, Target target, Arena scratch) {
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
        .scratch = scratch,
        .stream = stream,
    };

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
        ctx.tir.thread = &input->insts[i];
        ValueId value = input->declarations.functions.ptr[i];
        gen_function(&ctx, input->mir_result->ends[i], input->mir_result->ends[i + 1], value, input->declarations.main.id == value.id);
    }

    for (int32_t i = 0; i < ctx.strings.len; i++) {
        gen_string(&ctx, i, ctx.strings.ptr[i]);
    }

    fclose(stream);
}
