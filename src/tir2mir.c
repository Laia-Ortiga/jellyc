#include "tir2mir.h"

#include "arena.h"
#include "data/mir.h"
#include "data/tir.h"
#include "util.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    TirContext tir;
    Mir mir;
    int32_t basic_block;
    Vec(int32_t) break_instructions;
    Vec(int32_t) continue_instructions;
    Arena scratch;
    bool error;
} Context;

static int32_t add_br_instruction(Context *c, MirTag tag) {
    int32_t id = c->mir.data.len;
    vec_push(&c->mir.insts, tag);
    vec_push(&c->mir.data, 0);
    c->basic_block++;
    return id;
}

static void patch_br(Context *c, int32_t br, int32_t basic_block) {
    c->mir.data.ptr[br] = basic_block;
}

static void transform_node(Context *c, TirId tir_id);

static void transform_let(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId var = {data->b};
    TirId init = {data->c};
    vec_push(&c->mir.insts, MIR_ALLOC_VAR);
    vec_push(&c->mir.data, var.id);
    transform_node(c, init);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_plus(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
}

static void transform_unary(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
    vec_push(&c->mir.insts, tag);
}

static void transform_deref(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
    vec_push(&c->mir.insts, MIR_DEREF);
}

static void transform_cast(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
    TirId cast_type = get_value_type(c->tir, tir_id);
    vec_push(&c->mir.insts, tag);
    vec_push(&c->mir.data, cast_type.id);
}

static void transform_nop(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
}

static void transform_tmp_address(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId type = get_value_type(c->tir, operand);
    vec_push(&c->mir.insts, MIR_ALLOC);
    vec_push(&c->mir.data, type.id);
    vec_push(&c->mir.insts, MIR_STACK_COPY);
    transform_node(c, operand);
    vec_push(&c->mir.insts, MIR_ASSIGN);
    vec_push(&c->mir.insts, MIR_ADDRESS);
    vec_push(&c->mir.data, get_value_type(c->tir, tir_id).id);
}

static void transform_address(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    transform_node(c, operand);
    vec_push(&c->mir.insts, MIR_ADDRESS);
    vec_push(&c->mir.data, get_value_type(c->tir, tir_id).id);
}

static void transform_binary(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId left = {data->b};
    TirId right = {data->c};
    transform_node(c, left);
    transform_node(c, right);
    vec_push(&c->mir.insts, tag);
}

static void transform_compound_assignment(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId left = {data->b};
    TirId right = {data->c};
    transform_node(c, left);
    vec_push(&c->mir.insts, MIR_STACK_COPY);
    transform_node(c, right);
    vec_push(&c->mir.insts, tag);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_access(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t index = data->c;
    transform_node(c, operand);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, index);
}

static void transform_call(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t args = data->c;
    TirId type = get_value_type(c->tir, operand);
    FunctionType function_type = get_function_type(c->tir, type);
    transform_node(c, operand);

    for (int32_t i = 0; i < function_type.param_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        transform_node(c, arg);
    }

    vec_push(&c->mir.insts, MIR_CALL);
    vec_push(&c->mir.data, type.id);
}

static void transform_index(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId index = {data->c};
    TirId type = get_value_type(c->tir, operand);
    transform_node(c, operand);
    transform_node(c, index);
    MirTag tag = MIR_INDEX;
    if (remove_slice(c->tir, type).id) {
        tag = MIR_SLICE_INDEX;
    }
    vec_push(&c->mir.insts, tag);
}

static void transform_slice(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t index = data->c;
    TirId low = {get_term_extra(c->tir, index)};
    TirId high = {get_term_extra(c->tir, index + 1)};

    vec_push(&c->mir.insts, MIR_ALLOC);
    vec_push(&c->mir.data, get_value_type(c->tir, tir_id).id);

    transform_node(c, operand);
    transform_node(c, low);
    transform_node(c, high);

    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -4);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 0);
    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -2);
    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -4);
    vec_push(&c->mir.insts, MIR_SUB);
    vec_push(&c->mir.insts, MIR_ASSIGN);

    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -4);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 1);
    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -4);

    vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
    vec_push(&c->mir.data, -4);
    TirId operand_type = get_value_type(c->tir, operand);
    MirTag tag = MIR_INDEX;
    if (remove_slice(c->tir, operand_type).id) {
        tag = MIR_SLICE_INDEX;
    }
    vec_push(&c->mir.insts, tag);
    vec_push(&c->mir.insts, MIR_ADDRESS);
    vec_push(&c->mir.data, get_any_struct_type_field(c->tir, get_value_type(c->tir, tir_id), 1).id);
    vec_push(&c->mir.insts, MIR_ASSIGN);

    vec_push(&c->mir.insts, MIR_STACK_POP);
    vec_push(&c->mir.insts, MIR_STACK_POP);
    vec_push(&c->mir.insts, MIR_STACK_POP);
}

static void transform_array_to_slice(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId index_type = {data->c};
    int64_t length = get_array_length_type(c->tir, index_type);

    vec_push(&c->mir.insts, MIR_ALLOC);
    vec_push(&c->mir.data, get_value_type(c->tir, tir_id).id);

    vec_push(&c->mir.insts, MIR_STACK_COPY);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 0);
    vec_push(&c->mir.insts, MIR_INT);
    uint32_t length_low;
    uint32_t length_high;
    store_i64(length, &length_low, &length_high);
    vec_push(&c->mir.data, length_low);
    vec_push(&c->mir.data, length_high);
    vec_push(&c->mir.insts, MIR_ASSIGN);

    vec_push(&c->mir.insts, MIR_STACK_COPY);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 1);
    transform_node(c, operand);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_new_struct(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    int32_t args = data->b;
    int32_t arg_count = data->c;
    TirId type = get_value_type(c->tir, tir_id);
    vec_push(&c->mir.insts, MIR_ALLOC);
    vec_push(&c->mir.data, type.id);

    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        vec_push(&c->mir.insts, MIR_STACK_COPY);
        vec_push(&c->mir.insts, MIR_ACCESS);
        vec_push(&c->mir.data, i);
        transform_node(c, arg);
        vec_push(&c->mir.insts, MIR_ASSIGN);
    }
}

static void transform_new_array(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    int32_t args = data->b;
    int32_t arg_count = data->c;
    TirId type = get_value_type(c->tir, tir_id);

    vec_push(&c->mir.insts, MIR_ALLOC);
    vec_push(&c->mir.data, type.id);

    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        vec_push(&c->mir.insts, MIR_STACK_COPY);
        vec_push(&c->mir.insts, MIR_INT);
        vec_push(&c->mir.data, i);
        vec_push(&c->mir.data, 0);
        vec_push(&c->mir.insts, MIR_INDEX);
        transform_node(c, arg);
        vec_push(&c->mir.insts, MIR_ASSIGN);
    }
}

static bool last_is_terminator(Context *c, int32_t last_br) {
    return c->mir.insts.len - 1 > last_br && is_mir_terminator(c->mir.insts.ptr[c->mir.insts.len - 1]);
}

static void transform_statement(Context *c, TirId tir_id) {
    transform_node(c, tir_id);
    TirId type = get_value_type(c->tir, tir_id);

    if (type.id && type.id != TYPE_VOID) {
        vec_push(&c->mir.insts, MIR_STACK_POP);
    }
}

static void transform_if(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId condition = {data->b};
    int32_t extra = data->c;
    int32_t true_block = get_term_extra(c->tir, extra);
    int32_t true_block_length = get_term_extra(c->tir, extra + 1);
    int32_t false_block = get_term_extra(c->tir, extra + 2);
    int32_t false_block_length = get_term_extra(c->tir, extra + 3);
    transform_node(c, condition);
    int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);

    for (int32_t i = 0; i < true_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, true_block + i)};
        transform_statement(c, statement);
    }

    int32_t true_br = condition_br;

    if (!last_is_terminator(c, condition_br)) {
        true_br = add_br_instruction(c, MIR_BR);
    }

    int32_t false_basic_block = c->basic_block;

    for (int32_t i = 0; i < false_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, false_block + i)};
        transform_statement(c, statement);
    }

    int32_t false_br = condition_br;

    if (!last_is_terminator(c, true_br)) {
        false_br = add_br_instruction(c, MIR_BR);
    }

    int32_t next_basic_block = c->basic_block;
    patch_br(c, condition_br, false_basic_block);

    if (true_br != condition_br) {
        patch_br(c, true_br, next_basic_block);
    }

    if (false_br != condition_br) {
        patch_br(c, false_br, next_basic_block);
    }
}

static void transform_switch(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId switch_ = {data->b};
    int32_t extra = data->c;
    int32_t branches = get_term_extra(c->tir, extra);
    int32_t branch_count = get_term_extra(c->tir, extra + 1);
    TirId type = get_value_type(c->tir, tir_id);

    if (type.id != TYPE_VOID) {
        vec_push(&c->mir.insts, MIR_ALLOC);
        vec_push(&c->mir.data, type.id);
    }

    int32_t copy_inst = -1;

    if (switch_.id) {
        copy_inst = -2;
        transform_node(c, switch_);
    }

    int32_t *br_list = arena_alloc(&c->scratch, int32_t, branch_count);
    int32_t real_count = 0;

    for (int32_t i = 0; i < branch_count; i++) {
        TirId pattern = {get_term_extra(c->tir, branches + i * 2)};
        TirId value = {get_term_extra(c->tir, branches + i * 2 + 1)};

        if (pattern.id) {
            if (switch_.id) {
                vec_push(&c->mir.insts, MIR_STACK_COPY);
                transform_node(c, pattern);
                vec_push(&c->mir.insts, MIR_EQ);
            } else {
                transform_node(c, pattern);
            }

            int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);
            if (type.id != TYPE_VOID) {
                vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
                vec_push(&c->mir.data, copy_inst);
                transform_node(c, value);
                vec_push(&c->mir.insts, MIR_ASSIGN);
            } else {
                transform_statement(c, value);
            }
            br_list[real_count++] = add_br_instruction(c, MIR_BR);
            int32_t next_case_basic_block = c->basic_block;
            patch_br(c, condition_br, next_case_basic_block);
        } else {
            if (type.id != TYPE_VOID) {
                vec_push(&c->mir.insts, MIR_STACK_COPY_AT);
                vec_push(&c->mir.data, copy_inst);
                transform_node(c, value);
                vec_push(&c->mir.insts, MIR_ASSIGN);
            } else {
                transform_statement(c, value);
            }
            br_list[real_count++] = add_br_instruction(c, MIR_BR);
            break;
        }

        pattern = value;
    }

    for (int32_t i = 0; i < real_count; i++) {
        patch_br(c, br_list[i], c->basic_block);
    }

    if (switch_.id) {
        vec_push(&c->mir.insts, MIR_STACK_POP);
    }
}

static void transform_loop(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId condition = {data->b};
    int32_t extra = data->c;
    TirId let = {get_term_extra(c->tir, extra)};
    if (let.id) {
        transform_statement(c, let);
    }
    TirId next = {get_term_extra(c->tir, extra + 1)};
    int32_t block = get_term_extra(c->tir, extra + 2);
    int32_t block_length = get_term_extra(c->tir, extra + 3);

    int32_t entry_br = add_br_instruction(c, MIR_BR);
    int32_t condition_basic_block = c->basic_block;
    patch_br(c, entry_br, condition_basic_block);

    transform_node(c, condition);
    int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);

    int32_t break_index = c->break_instructions.len;
    int32_t continue_index = c->continue_instructions.len;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        transform_statement(c, statement);
    }

    int32_t continue_basic_block = condition_basic_block;

    if (next.id) {
        int32_t continue_br = add_br_instruction(c, MIR_BR);
        continue_basic_block = c->basic_block;
        patch_br(c, continue_br, continue_basic_block);
        transform_statement(c, next);
    }

    int32_t next_iteration_br = add_br_instruction(c, MIR_BR);
    int32_t exit_basic_block = c->basic_block;
    patch_br(c, next_iteration_br, condition_basic_block);
    patch_br(c, condition_br, exit_basic_block);

    for (int32_t i = break_index; i < c->break_instructions.len; i++) {
        patch_br(c, c->break_instructions.ptr[i], exit_basic_block);
    }

    for (int32_t i = continue_index; i < c->continue_instructions.len; i++) {
        patch_br(c, c->continue_instructions.ptr[i], continue_basic_block);
    }

    c->break_instructions.len = break_index;
    c->continue_instructions.len = continue_index;
}

static void transform_break(Context *c) {
    int32_t inst = add_br_instruction(c, MIR_BR);
    vec_push(&c->break_instructions, inst);
}

static void transform_continue(Context *c) {
    int32_t inst = add_br_instruction(c, MIR_BR);
    vec_push(&c->continue_instructions, inst);
}

static void transform_return(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};

    if (operand.id) {
        transform_node(c, operand);
        c->basic_block++;
        vec_push(&c->mir.insts, MIR_RET);
        return;
    }

    c->basic_block++;
    vec_push(&c->mir.insts, MIR_RET_VOID);
}

static void transform_function(Context *c, int32_t block, int32_t block_length, TirId value) {
    TirId type = get_value_type(c->tir, value);
    FunctionType func_type = get_function_type(c->tir, type);
    int32_t start = c->mir.insts.len;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        transform_statement(c, statement);
    }

    if (func_type.ret.id == TYPE_VOID && (c->mir.insts.len == start || c->mir.insts.ptr[c->mir.insts.len - 1] != MIR_RET_VOID)) {
        vec_push(&c->mir.insts, MIR_RET_VOID);
    }
}

static void transform_node(Context *c, TirId tir_id) {
    switch (get_term_tag(c->tir, tir_id)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL:
        case TIR_STRING:
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            vec_push(&c->mir.insts, MIR_TIR_VALUE);
            vec_push(&c->mir.data, tir_id.id);
            break;
        }
        case TIR_LET: transform_let(c, tir_id); break;
        case TIR_PLUS: transform_plus(c, tir_id); break;
        case TIR_MINUS: transform_unary(c, tir_id, MIR_NEG); break;
        case TIR_NOT: transform_unary(c, tir_id, MIR_NOT); break;
        case TIR_DEREF: transform_deref(c, tir_id); break;
        case TIR_ADDRESS_OF_TEMPORARY: transform_tmp_address(c, tir_id); break;
        case TIR_ADDRESS: transform_address(c, tir_id); break;
        case TIR_ADD: transform_binary(c, tir_id, MIR_ADD); break;
        case TIR_SUB: transform_binary(c, tir_id, MIR_SUB); break;
        case TIR_MUL: transform_binary(c, tir_id, MIR_MUL); break;
        case TIR_DIV: transform_binary(c, tir_id, MIR_DIV); break;
        case TIR_MOD: transform_binary(c, tir_id, MIR_MOD); break;
        case TIR_AND: transform_binary(c, tir_id, MIR_AND); break;
        case TIR_OR: transform_binary(c, tir_id, MIR_OR); break;
        case TIR_XOR: transform_binary(c, tir_id, MIR_XOR); break;
        case TIR_SHL: transform_binary(c, tir_id, MIR_SHL); break;
        case TIR_SHR: transform_binary(c, tir_id, MIR_SHR); break;
        case TIR_EQ: transform_binary(c, tir_id, MIR_EQ); break;
        case TIR_NE: transform_binary(c, tir_id, MIR_NE); break;
        case TIR_LT: transform_binary(c, tir_id, MIR_LT); break;
        case TIR_GT: transform_binary(c, tir_id, MIR_GT); break;
        case TIR_LE: transform_binary(c, tir_id, MIR_LE); break;
        case TIR_GE: transform_binary(c, tir_id, MIR_GE); break;
        case TIR_ASSIGN: transform_binary(c, tir_id, MIR_ASSIGN); break;
        case TIR_ASSIGN_ADD: transform_compound_assignment(c, tir_id, MIR_ADD); break;
        case TIR_ASSIGN_SUB: transform_compound_assignment(c, tir_id, MIR_SUB); break;
        case TIR_ASSIGN_MUL: transform_compound_assignment(c, tir_id, MIR_MUL); break;
        case TIR_ASSIGN_DIV: transform_compound_assignment(c, tir_id, MIR_DIV); break;
        case TIR_ASSIGN_MOD: transform_compound_assignment(c, tir_id, MIR_MOD); break;
        case TIR_ASSIGN_AND: transform_compound_assignment(c, tir_id, MIR_AND); break;
        case TIR_ASSIGN_OR: transform_compound_assignment(c, tir_id, MIR_OR); break;
        case TIR_ASSIGN_XOR: transform_compound_assignment(c, tir_id, MIR_XOR); break;
        case TIR_ACCESS: transform_access(c, tir_id); break;
        case TIR_ITOF: transform_cast(c, tir_id, MIR_ITOF); break;
        case TIR_ITRUNC: transform_cast(c, tir_id, MIR_ITRUNC); break;
        case TIR_SEXT: transform_cast(c, tir_id, MIR_SEXT); break;
        case TIR_ZEXT: transform_cast(c, tir_id, MIR_ZEXT); break;
        case TIR_FTOI: transform_cast(c, tir_id, MIR_FTOI); break;
        case TIR_FTRUNC: transform_cast(c, tir_id, MIR_FTRUNC); break;
        case TIR_FEXT: transform_cast(c, tir_id, MIR_FEXT); break;
        case TIR_PTR_CAST: transform_cast(c, tir_id, MIR_PTR_CAST); break;
        case TIR_NOP: transform_nop(c, tir_id); break;
        case TIR_ARRAY_TO_SLICE: transform_array_to_slice(c, tir_id); break;
        case TIR_CALL: transform_call(c, tir_id); break;
        case TIR_INDEX: transform_index(c, tir_id); break;
        case TIR_SLICE: transform_slice(c, tir_id); break;
        case TIR_NEW_STRUCT: transform_new_struct(c, tir_id); break;
        case TIR_NEW_ARRAY: transform_new_array(c, tir_id); break;
        case TIR_IF: transform_if(c, tir_id); break;
        case TIR_SWITCH: transform_switch(c, tir_id); break;
        case TIR_LOOP: transform_loop(c, tir_id); break;
        case TIR_BREAK: transform_break(c); break;
        case TIR_CONTINUE: transform_continue(c); break;
        case TIR_RETURN: transform_return(c, tir_id); break;
        default: compiler_error("tir_to_mir: unimplemented tag");
    }
}

MirResult tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch) {
    Mir mir = {0};
    int32_t *ends = arena_alloc(permanent, int32_t, input->function_count + 1);
    int32_t *data_starts = arena_alloc(permanent, int32_t, input->function_count);

    for (int32_t i = 0; i < input->function_count; i++) {
        Context c = {0};
        c.mir = mir;
        c.tir.global = input->global_deps;
        c.tir.thread = &input->insts[i].deps;
        c.scratch = scratch;
        ends[i] = c.mir.insts.len;
        data_starts[i] = c.mir.data.len;
        transform_function(&c, input->insts[i].body_first, input->insts[i].body_length, input->functions[i]);
        free(c.break_instructions.ptr);
        free(c.continue_instructions.ptr);
        mir = c.mir;
        ends[i + 1] = c.mir.insts.len;
    }

    return (MirResult) {
        .mir = mir,
        .ends = ends,
        .data_starts = data_starts,
    };
}
