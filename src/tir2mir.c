#include "tir2mir.h"

#include "arena.h"
#include "data/mir.h"
#include "data/tir.h"
#include "util.h"
#include "wrappers.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    TirContext tir;
    Mir mir;
    MirId *variable_to_mir_map;
    int32_t basic_block;
    Vec(MirId) break_instructions;
    Vec(MirId) continue_instructions;
    Arena scratch;
    bool error;
} Context;

int32_t push_extra(Context *c, int32_t *extra, int32_t count) {
    int32_t index = c->mir.extra.len;
    int32_t *destination = vec_grow(&c->mir.extra, count);
    memcpy(destination, extra, count * sizeof(int32_t));
    return index;
}

static MirId add_leaf_instruction(Context *c, MirTag tag, TirId type) {
    MirData data = {0};
    data.type = type;
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    return id;
}

static MirId add_value_instruction(Context *c, MirTag tag, TirId value) {
    TirId type = get_value_type(c->tir, value);
    MirData data = {type, .tir_value = value};
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    return id;
}

static MirId add_binary_instruction(Context *c, MirTag tag, TirId type, MirId left, MirId right) {
    MirData data = {type, .binary = {left, right}};
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    return id;
}

static MirId add_unary_instruction(Context *c, MirTag tag, TirId type, MirId operand) {
    MirData data = {type, .unary = operand};
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    return id;
}

static MirId add_mir_const_instruction(Context *c, MirTag tag, TirId type, MirId operand, int32_t index) {
    MirData data = {type, .mir_const = {operand, index}};
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    return id;
}

static MirId add_int_instruction(Context *c, TirId type, int64_t i) {
    MirData data = {type, .raw = {(int32_t) (uint32_t) i, (int32_t) (uint32_t) ((uint64_t) i >> 32)}};
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, MIR_INT);
    return id;
}

static MirId add_cond_br_instruction(Context *c, MirTag tag, MirId operand) {
    MirData data = {0};
    data.type = null_tir;
    data.mir_const.operand = operand;
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, tag);
    c->basic_block++;
    return id;
}

static MirId add_br_instruction(Context *c) {
    MirData data = {0};
    data.type = null_tir;
    MirId id = {c->mir.mir.len};
    sum_vec_push(&c->mir.mir, data, MIR_BR);
    c->basic_block++;
    return id;
}

static void patch_br(Context *c, MirId br, int32_t basic_block) {
    c->mir.mir.datas[br.private_field_id].mir_const.index = basic_block;
}

static MirId transform_node(Context *c, TirId tir_id);

static MirId transform_let(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    int32_t variable = data->b;
    TirId init = {data->c};
    TirId type = get_value_type(c->tir, init);
    MirId alloc_mir = add_leaf_instruction(c, MIR_ALLOC, type);
    MirId init_mir = transform_node(c, init);
    add_binary_instruction(c, MIR_ASSIGN, type, alloc_mir, init_mir);
    c->variable_to_mir_map[variable] = alloc_mir;
    return alloc_mir;
}

static MirId transform_plus(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    return transform_node(c, operand);
}

static MirId transform_unary(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId type = get_value_type(c->tir, operand);
    MirId operand_mir = transform_node(c, operand);
    return add_unary_instruction(c, tag, type, operand_mir);
}

static MirId transform_deref(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    MirId operand_mir = transform_node(c, operand);
    TirId deref_type = get_value_type(c->tir, tir_id);
    return add_unary_instruction(c, MIR_DEREF, deref_type, operand_mir);
}

static MirId transform_cast(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId type = get_value_type(c->tir, operand);
    MirId operand_mir = transform_node(c, operand);
    TirId cast_type = get_value_type(c->tir, tir_id);
    return add_mir_const_instruction(c, tag, cast_type, operand_mir, type.id);
}

static MirId transform_nop(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    return transform_node(c, operand);
}

static MirId transform_tmp_address(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId type = get_value_type(c->tir, operand);
    MirId alloc_mir = add_leaf_instruction(c, MIR_ALLOC, type);
    MirId operand_mir = transform_node(c, operand);
    add_binary_instruction(c, MIR_ASSIGN, type, alloc_mir, operand_mir);
    return add_unary_instruction(c, MIR_ADDRESS, type, alloc_mir);
}

static MirId transform_address(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId type = get_value_type(c->tir, operand);
    MirId operand_mir = transform_node(c, operand);
    return add_unary_instruction(c, MIR_ADDRESS, type, operand_mir);
}

static MirId transform_binary(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId left = {data->b};
    TirId right = {data->c};
    TirId type = get_value_type(c->tir, left);
    MirId left_mir = transform_node(c, left);
    MirId right_mir = transform_node(c, right);
    return add_binary_instruction(c, tag, type, left_mir, right_mir);
}

static MirId transform_compound_assignment(Context *c, TirId tir_id, MirTag tag) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId left = {data->b};
    TirId right = {data->c};
    TirId type = get_value_type(c->tir, left);
    MirId left_mir = transform_node(c, left);
    MirId right_mir = transform_node(c, right);
    MirId op_result = add_binary_instruction(c, tag, type, left_mir, right_mir);
    return add_binary_instruction(c, MIR_ASSIGN, type, left_mir, op_result);
}

static MirId transform_access(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t index = data->c;
    TirId type = get_value_type(c->tir, operand);
    TirId s = remove_tags(c->tir, type);
    MirId operand_mir = transform_node(c, operand);
    return add_mir_const_instruction(c, MIR_ACCESS, s, operand_mir, index);
}

static MirId transform_call(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t args = data->c;
    TirId type = get_value_type(c->tir, operand);
    FunctionType function_type = get_function_type(c->tir, type);
    MirId operand_mir = transform_node(c, operand);

    int32_t *args_mir = arena_alloc(&c->scratch, int32_t, function_type.param_count);

    for (int32_t i = 0; i < function_type.param_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        MirId arg_mir = transform_node(c, arg);
        args_mir[i] = arg_mir.private_field_id;
    }

    return add_mir_const_instruction(c, MIR_CALL, type, operand_mir, push_extra(c, args_mir, function_type.param_count));
}

static MirId transform_index(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId index = {data->c};
    TirId type = get_value_type(c->tir, operand);
    MirId operand_mir = transform_node(c, operand);
    MirId index_mir = transform_node(c, index);
    MirTag tag = MIR_INDEX;
    if (remove_slice(c->tir, type).id) {
        tag = MIR_SLICE_INDEX;
    }
    return add_binary_instruction(c, tag, type, operand_mir, index_mir);
}

static MirId transform_slice(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    int32_t index = data->c;
    TirId low = {get_term_extra(c->tir, index)};
    TirId high = {get_term_extra(c->tir, index + 1)};
    MirId operand_mir = transform_node(c, operand);
    MirId low_mir = transform_node(c, low);
    MirId high_mir = transform_node(c, high);
    MirId length_mir = add_binary_instruction(c, MIR_SUB, type_isize, high_mir, low_mir);
    TirId operand_type = get_value_type(c->tir, operand);
    MirTag tag = MIR_INDEX;
    if (remove_slice(c->tir, operand_type).id) {
        tag = MIR_SLICE_INDEX;
    }
    MirId data_mir = add_binary_instruction(c, tag, operand_type, operand_mir, low_mir);
    return add_binary_instruction(c, MIR_NEW_SLICE, get_value_type(c->tir, tir_id), length_mir, data_mir);
}

static MirId transform_array_to_slice(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};
    TirId index_type = {data->c};
    int64_t length = get_array_length_type(c->tir, index_type);
    MirId length_mir = add_int_instruction(c, type_isize, length);
    MirId data_mir = transform_node(c, operand);
    return add_binary_instruction(c, MIR_NEW_SLICE, get_value_type(c->tir, tir_id), length_mir, data_mir);
}

static MirId transform_new_struct(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    int32_t args = data->b;
    int32_t arg_count = data->c;
    TirId type = get_value_type(c->tir, tir_id);
    MirId alloc_mir = add_leaf_instruction(c, MIR_ALLOC, type);
    TirId s = remove_tags(c->tir, type);

    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        TirId field_type = get_value_type(c->tir, arg);
        MirId arg_mir = transform_node(c, arg);
        MirId field_address = add_mir_const_instruction(c, MIR_ACCESS, s, alloc_mir, i);
        add_binary_instruction(c, MIR_ASSIGN, field_type, field_address, arg_mir);
    }

    return alloc_mir;
}

static MirId transform_new_array(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    int32_t args = data->b;
    int32_t arg_count = data->c;
    TirId type = get_value_type(c->tir, tir_id);
    MirId alloc_mir = add_leaf_instruction(c, MIR_ALLOC, type);
    TirId element_type = remove_c_pointer_like(c->tir, type);

    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        MirId arg_mir = transform_node(c, arg);
        MirId field_address = add_mir_const_instruction(c, MIR_CONST_INDEX, type, alloc_mir, i);
        add_binary_instruction(c, MIR_ASSIGN, element_type, field_address, arg_mir);
    }

    return alloc_mir;
}

static bool last_is_terminator(Context *c, MirId last_br) {
    return c->mir.mir.len - 1 > last_br.private_field_id && is_mir_terminator(c->mir.mir.tags[c->mir.mir.len - 1]);
}

static MirId transform_if(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId condition = {data->b};
    int32_t extra = data->c;
    int32_t true_block = get_term_extra(c->tir, extra);
    int32_t true_block_length = get_term_extra(c->tir, extra + 1);
    int32_t false_block = get_term_extra(c->tir, extra + 2);
    int32_t false_block_length = get_term_extra(c->tir, extra + 3);
    MirId condition_mir = transform_node(c, condition);
    MirId condition_br = add_cond_br_instruction(c, MIR_BR_IF_NOT, condition_mir);

    for (int32_t i = 0; i < true_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, true_block + i)};
        transform_node(c, statement);
    }

    MirId true_br = condition_br;

    if (!last_is_terminator(c, condition_br)) {
        true_br = add_br_instruction(c);
    }

    int32_t false_basic_block = c->basic_block;

    for (int32_t i = 0; i < false_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, false_block + i)};
        transform_node(c, statement);
    }

    MirId false_br = condition_br;

    if (!last_is_terminator(c, true_br)) {
        false_br = add_br_instruction(c);
    }

    int32_t next_basic_block = c->basic_block;
    patch_br(c, condition_br, false_basic_block);

    if (true_br.private_field_id != condition_br.private_field_id) {
        patch_br(c, true_br, next_basic_block);
    }

    if (false_br.private_field_id != condition_br.private_field_id) {
        patch_br(c, false_br, next_basic_block);
    }

    return condition_mir;
}

static MirId transform_switch(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId switch_ = {data->b};
    int32_t extra = data->c;
    int32_t branches = get_term_extra(c->tir, extra);
    int32_t branch_count = get_term_extra(c->tir, extra + 1);
    TirId type = get_value_type(c->tir, tir_id);
    MirId alloc_mir = {0};

    if (type.id != TYPE_VOID) {
        alloc_mir = add_leaf_instruction(c, MIR_ALLOC, type);
    }

    MirId switch_mir;
    TirId pattern_type;

    if (switch_.id) {
        switch_mir = transform_node(c, switch_);
        pattern_type = get_value_type(c->tir, switch_);
    }

    MirId *br_list = arena_alloc(&c->scratch, MirId, branch_count);
    int32_t real_count = 0;

    for (int32_t i = 0; i < branch_count; i++) {
        TirId pattern = {get_term_extra(c->tir, branches + i * 2)};
        TirId value = {get_term_extra(c->tir, branches + i * 2 + 1)};

        if (pattern.id) {
            MirId pattern_mir = transform_node(c, pattern);
            MirId condition_mir = pattern_mir;

            if (switch_.id) {
                condition_mir = add_binary_instruction(c, MIR_EQ, pattern_type, switch_mir, pattern_mir);
            }

            MirId condition_br = add_cond_br_instruction(c, MIR_BR_IF_NOT, condition_mir);
            MirId value_mir = transform_node(c, value);
            if (type.id != TYPE_VOID) {
                add_binary_instruction(c, MIR_ASSIGN, type, alloc_mir, value_mir);
            }
            br_list[real_count++] = add_br_instruction(c);
            int32_t next_case_basic_block = c->basic_block;
            patch_br(c, condition_br, next_case_basic_block);
        } else {
            MirId value_mir = transform_node(c, value);
            if (type.id != TYPE_VOID) {
                add_binary_instruction(c, MIR_ASSIGN, type, alloc_mir, value_mir);
            }
            br_list[real_count++] = add_br_instruction(c);
            break;
        }

        pattern = value;
    }

    for (int32_t i = 0; i < real_count; i++) {
        patch_br(c, br_list[i], c->basic_block);
    }

    return alloc_mir;
}

static MirId transform_loop(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId condition = {data->b};
    int32_t extra = data->c;
    TirId next = {get_term_extra(c->tir, extra)};
    int32_t block = get_term_extra(c->tir, extra + 1);
    int32_t block_length = get_term_extra(c->tir, extra + 2);

    MirId entry_br = add_br_instruction(c);
    int32_t condition_basic_block = c->basic_block;
    patch_br(c, entry_br, condition_basic_block);

    MirId condition_mir = transform_node(c, condition);
    MirId condition_br = add_cond_br_instruction(c, MIR_BR_IF_NOT, condition_mir);

    int32_t break_index = c->break_instructions.len;
    int32_t continue_index = c->continue_instructions.len;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        transform_node(c, statement);
    }

    int32_t continue_basic_block = condition_basic_block;

    if (next.id) {
        MirId continue_br = add_br_instruction(c);
        continue_basic_block = c->basic_block;
        patch_br(c, continue_br, continue_basic_block);
        transform_node(c, next);
    }

    MirId next_iteration_br = add_br_instruction(c);
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
    return condition_mir;
}

static MirId transform_break(Context *c) {
    MirId mir = add_br_instruction(c);
    vec_push(&c->break_instructions, mir);
    return mir;
}

static MirId transform_continue(Context *c) {
    MirId mir = add_br_instruction(c);
    vec_push(&c->continue_instructions, mir);
    return mir;
}

static MirId transform_return(Context *c, TirId tir_id) {
    TermData const *data = get_term_data(c->tir, tir_id);
    TirId operand = {data->b};

    if (operand.id) {
        TirId type = get_value_type(c->tir, operand);
        MirId operand_mir = transform_node(c, operand);
        c->basic_block++;
        return add_unary_instruction(c, MIR_RET, type, operand_mir);
    }

    c->basic_block++;
    return add_leaf_instruction(c, MIR_RET_VOID, null_tir);
}

static void transform_function(Context *c, int32_t block, int32_t block_length, TirId value) {
    TirId type = get_value_type(c->tir, value);
    FunctionType func_type = get_function_type(c->tir, type);

    for (int32_t i = 0; i < func_type.param_count; i++) {
        TirId param_type = get_function_type_param(c->tir, type, i);
        c->variable_to_mir_map[i] = add_leaf_instruction(c, MIR_PARAM, param_type);
    }

    int32_t start = c->mir.mir.len;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        transform_node(c, statement);
    }

    if (func_type.ret.id == TYPE_VOID && (c->mir.mir.len == start || c->mir.mir.tags[c->mir.mir.len - 1] != MIR_RET_VOID)) {
        add_leaf_instruction(c, MIR_RET_VOID, null_tir);
    }
}

static MirId transform_node(Context *c, TirId tir_id) {
    switch (get_term_tag(c->tir, tir_id)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL: {
            return add_value_instruction(c, MIR_TIR_VALUE, tir_id);
        }
        case TIR_STRING: {
            return add_value_instruction(c, MIR_STRING, tir_id);
        }
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            int32_t variable = get_term_data(c->tir, tir_id)->b;
            return c->variable_to_mir_map[variable];
        }
        case TIR_LET: return transform_let(c, tir_id);
        case TIR_MUT: return transform_let(c, tir_id);
        case TIR_PLUS: return transform_plus(c, tir_id);
        case TIR_MINUS: return transform_unary(c, tir_id, MIR_MINUS);
        case TIR_NOT: return transform_unary(c, tir_id, MIR_NOT);
        case TIR_DEREF: return transform_deref(c, tir_id);
        case TIR_ADDRESS_OF_TEMPORARY: return transform_tmp_address(c, tir_id);
        case TIR_ADDRESS: return transform_address(c, tir_id);
        case TIR_ADD: return transform_binary(c, tir_id, MIR_ADD);
        case TIR_SUB: return transform_binary(c, tir_id, MIR_SUB);
        case TIR_MUL: return transform_binary(c, tir_id, MIR_MUL);
        case TIR_DIV: return transform_binary(c, tir_id, MIR_DIV);
        case TIR_MOD: return transform_binary(c, tir_id, MIR_MOD);
        case TIR_AND: return transform_binary(c, tir_id, MIR_AND);
        case TIR_OR: return transform_binary(c, tir_id, MIR_OR);
        case TIR_XOR: return transform_binary(c, tir_id, MIR_XOR);
        case TIR_SHL: return transform_binary(c, tir_id, MIR_SHL);
        case TIR_SHR: return transform_binary(c, tir_id, MIR_SHR);
        case TIR_EQ: return transform_binary(c, tir_id, MIR_EQ);
        case TIR_NE: return transform_binary(c, tir_id, MIR_NE);
        case TIR_LT: return transform_binary(c, tir_id, MIR_LT);
        case TIR_GT: return transform_binary(c, tir_id, MIR_GT);
        case TIR_LE: return transform_binary(c, tir_id, MIR_LE);
        case TIR_GE: return transform_binary(c, tir_id, MIR_GE);
        case TIR_ASSIGN: return transform_binary(c, tir_id, MIR_ASSIGN);
        case TIR_ASSIGN_ADD: return transform_compound_assignment(c, tir_id, MIR_ADD);
        case TIR_ASSIGN_SUB: return transform_compound_assignment(c, tir_id, MIR_SUB);
        case TIR_ASSIGN_MUL: return transform_compound_assignment(c, tir_id, MIR_MUL);
        case TIR_ASSIGN_DIV: return transform_compound_assignment(c, tir_id, MIR_DIV);
        case TIR_ASSIGN_MOD: return transform_compound_assignment(c, tir_id, MIR_MOD);
        case TIR_ASSIGN_AND: return transform_compound_assignment(c, tir_id, MIR_AND);
        case TIR_ASSIGN_OR: return transform_compound_assignment(c, tir_id, MIR_OR);
        case TIR_ASSIGN_XOR: return transform_compound_assignment(c, tir_id, MIR_XOR);
        case TIR_ACCESS: return transform_access(c, tir_id);
        case TIR_ITOF: return transform_cast(c, tir_id, MIR_ITOF);
        case TIR_ITRUNC: return transform_cast(c, tir_id, MIR_ITRUNC);
        case TIR_SEXT: return transform_cast(c, tir_id, MIR_SEXT);
        case TIR_ZEXT: return transform_cast(c, tir_id, MIR_ZEXT);
        case TIR_FTOI: return transform_cast(c, tir_id, MIR_FTOI);
        case TIR_FTRUNC: return transform_cast(c, tir_id, MIR_FTRUNC);
        case TIR_FEXT: return transform_cast(c, tir_id, MIR_FEXT);
        case TIR_PTR_CAST: return transform_cast(c, tir_id, MIR_PTR_CAST);
        case TIR_NOP: return transform_nop(c, tir_id);
        case TIR_ARRAY_TO_SLICE: return transform_array_to_slice(c, tir_id);
        case TIR_CALL: return transform_call(c, tir_id);
        case TIR_INDEX: return transform_index(c, tir_id);
        case TIR_SLICE: return transform_slice(c, tir_id);
        case TIR_NEW_STRUCT: return transform_new_struct(c, tir_id);
        case TIR_NEW_ARRAY: return transform_new_array(c, tir_id);
        case TIR_IF: return transform_if(c, tir_id);
        case TIR_SWITCH: return transform_switch(c, tir_id);
        case TIR_LOOP: return transform_loop(c, tir_id);
        case TIR_BREAK: return transform_break(c);
        case TIR_CONTINUE: return transform_continue(c);
        case TIR_RETURN: return transform_return(c, tir_id);
        default: abort();
    }
    compiler_error("tir_to_mir: unimplemented tag");
}

MirResult tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch) {
    Mir mir = {0};
    int32_t *ends = arena_alloc(permanent, int32_t, input->function_count + 1);

    for (int32_t i = 0; i < input->function_count; i++) {
        Context c = {0};
        c.mir = mir;
        c.tir.global = input->global_deps;
        c.tir.thread = &input->insts[i];
        c.scratch = scratch;
        c.variable_to_mir_map = arena_alloc(&c.scratch, MirId, input->insts[i].local_count);
        ends[i] = c.mir.mir.len;
        transform_function(&c, input->insts[i].body_first, input->insts[i].body_length, input->functions[i]);
        free(c.break_instructions.ptr);
        free(c.continue_instructions.ptr);
        mir = c.mir;
        ends[i + 1] = c.mir.mir.len;
    }

    return (MirResult) {
        .mir = mir,
        .ends = ends,
    };
}
