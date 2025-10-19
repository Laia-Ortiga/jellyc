#include "tir2mir.h"

#include "arena.h"
#include "fwd.h"
#include "mir.h"
#include "tir.h"
#include "type.h"
#include "util.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    TirContext tir;
    Target target;
    Mir mir;
    int32_t basic_block;
    MirTypeId *global_struct_types;
    MirTypeId *local_struct_types;
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

static MirTypeId transform_type(Context *c, TirId type) {
    switch (get_tir_tag(c->tir, type)) {
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case TYPE_VOID: {
                    return (MirTypeId) {MIR_TYPE_VOID};
                }
                case TYPE_i8:
                case TYPE_byte: {
                    return (MirTypeId) {MIR_TYPE_I8};
                }
                case TYPE_i16: {
                    return (MirTypeId) {MIR_TYPE_I16};
                }
                case TYPE_i32: {
                    return (MirTypeId) {MIR_TYPE_I32};
                }
                case TYPE_i64: {
                    return (MirTypeId) {MIR_TYPE_I64};
                }
                case TYPE_isize: {
                    switch (c->target) {
                        case TARGET_ISIZE_64: {
                            return (MirTypeId) {MIR_TYPE_I64};
                        }
                        case TARGET_ISIZE_32: {
                            return (MirTypeId) {MIR_TYPE_I32};
                        }
                        default: {
                            break;
                        }
                    }
                    break;
                }
                case TYPE_f32: {
                    return (MirTypeId) {MIR_TYPE_F32};
                }
                case TYPE_f64: {
                    return (MirTypeId) {MIR_TYPE_F64};
                }
                case TYPE_bool: {
                    return (MirTypeId) {MIR_TYPE_BOOL};
                }
                default: {
                    break;
                }
            }
            break;
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType t = tir_get_array_type(c->tir, type);
            int64_t length = tir_get_array_length_type(c->tir, t.index).length;
            vec_push(&c->mir.types, (MirTypeUnion) {
                .tag = MIR_TYPE_ARRAY,
                .array = {
                    .elem = transform_type(c, t.elem),
                    .length = length,
                }
            });
            return (MirTypeId) {c->mir.types.len - 1};
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return transform_type(c, ptype(isize));
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            return (MirTypeId) {MIR_TYPE_PTR};
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return (MirTypeId) {MIR_TYPE_SLICE};
        }
        case TIR_FUNCTION_TYPE: {
            TermIndex index = tir_get_storage(c->tir, type);
            if (index.tir == c->tir.global) {
                if (c->global_struct_types[index.index].private_field_id) {
                    return c->global_struct_types[index.index];
                }
            } else {
                if (c->local_struct_types[index.index].private_field_id) {
                    return c->local_struct_types[index.index];
                }
            }
            TirFunctionType t = tir_get_function_type(c->tir, type);
            int32_t first_field = c->mir.type_extra.len;
            vec_grow(&c->mir.type_extra, t.params.len);
            for (int32_t i = 0; i < t.params.len; i++) {
                MirTypeId field = transform_type(c, t.params.ptr[i]);
                c->mir.type_extra.ptr[first_field + i] = field;
            }
            MirTypeId ret = transform_type(c, t.ret);
            vec_push(&c->mir.types, (MirTypeUnion) {
                .tag = MIR_TYPE_FUNCTION,
                .function = {
                    .param_count = t.params.len,
                    .first_param = first_field,
                    .ret = ret,
                },
            });
            MirTypeId result = {c->mir.types.len - 1};
            if (index.tir == c->tir.global) {
                c->global_struct_types[index.index] = result;
            } else {
                c->local_struct_types[index.index] = result;
            }
            return result;
        }
        case TIR_TAGGED_TYPE: {
            return transform_type(c, remove_tags(c->tir, type));
        }
        case TIR_STRUCT_TYPE: {
            TermIndex index = tir_get_storage(c->tir, type);
            if (index.tir == c->tir.global) {
                if (c->global_struct_types[index.index].private_field_id) {
                    return c->global_struct_types[index.index];
                }
            } else {
                if (c->local_struct_types[index.index].private_field_id) {
                    return c->local_struct_types[index.index];
                }
            }
            TirStructType t = tir_get_struct_type(c->tir, type);
            int32_t first_field = c->mir.type_extra.len;
            vec_grow(&c->mir.type_extra, t.fields.len);
            for (int32_t i = 0; i < t.fields.len; i++) {
                MirTypeId field = transform_type(c, t.fields.ptr[i]);
                c->mir.type_extra.ptr[first_field + i] = field;
            }
            vec_push(&c->mir.types, (MirTypeUnion) {
                .tag = MIR_TYPE_STRUCT,
                .struct_ = {
                    .field_count = t.fields.len,
                    .first_field = first_field,
                    .alignment = t.alignment,
                    .size = t.size,
                },
            });
            MirTypeId result = {c->mir.types.len - 1};
            if (index.tir == c->tir.global) {
                c->global_struct_types[index.index] = result;
            } else {
                c->local_struct_types[index.index] = result;
            }
            return result;
        }
        case TIR_ENUM_TYPE: {
            return transform_type(c, tir_get_enum_type(c->tir, type).repr);
        }
        case TIR_AFFINE_TYPE: {
            return transform_type(c, tir_get_affine_type(c->tir, type).elem);
        }
        case TIR_TYPE_PARAMETER:
        default: {
            break;
        }
    }
    abort();
}

static void push_type(Context *c, MirTypeId type) {
    vec_push(&c->mir.data, type.private_field_id);
}

static void transform_node(Context *c, TirId tir_id);

static void transform_let(Context *c, TirId tir_id) {
    TirLet t = tir_get_let(c->tir, tir_id);
    TirVariable v = tir_get_variable(c->tir, t.var) ;
    vec_push(&c->mir.insts, MIR_ALLOC_VAR);
    push_type(c, transform_type(c, v.type));
    vec_push(&c->mir.data, v.index);
    transform_node(c, t.init);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_plus(Context *c, TirId tir_id) {
    TirUnary t = tir_get_unary(c->tir, tir_id);
    transform_node(c, t.a);
}

static void transform_unary(Context *c, TirId tir_id, MirTag tag) {
    TirUnary t = tir_get_unary(c->tir, tir_id);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, tag);
}

static void transform_deref(Context *c, TirId tir_id) {
    TirUnary t = tir_get_unary(c->tir, tir_id);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, MIR_DEREF);
    push_type(c, transform_type(c, t.type));
}

static void transform_cast(Context *c, TirId tir_id, MirTag tag) {
    TirCast t = tir_get_cast(c->tir, tir_id);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, tag);
    push_type(c, transform_type(c, t.type));
}

static void transform_nop(Context *c, TirId tir_id) {
    TirCast t = tir_get_cast(c->tir, tir_id);
    transform_node(c, t.a);
}

static void transform_tmp_address(Context *c, TirId tir_id) {
    TirUnary t = tir_get_unary(c->tir, tir_id);
    TirId type = get_value_type(c->tir, t.a);
    vec_push(&c->mir.insts, MIR_ALLOC);
    push_type(c, transform_type(c, type));
    vec_push(&c->mir.insts, MIR_STACK_COPY);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, MIR_ASSIGN);
    vec_push(&c->mir.insts, MIR_ADDRESS);
}

static void transform_address(Context *c, TirId tir_id) {
    TirUnary t = tir_get_unary(c->tir, tir_id);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, MIR_ADDRESS);
}

static void transform_binary(Context *c, TirId tir_id, MirTag tag) {
    TirBinary t = tir_get_binary(c->tir, tir_id);
    transform_node(c, t.a);
    transform_node(c, t.b);
    vec_push(&c->mir.insts, tag);
}

static void transform_compound_assignment(Context *c, TirId tir_id, MirTag tag) {
    TirBinary t = tir_get_binary(c->tir, tir_id);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, MIR_STACK_COPY);
    transform_node(c, t.b);
    vec_push(&c->mir.insts, tag);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_access(Context *c, TirId tir_id) {
    TirAccess t = tir_get_access(c->tir, tir_id);
    transform_node(c, t.s);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, t.field);
}

static void transform_call(Context *c, TirId tir_id) {
    TirCall t = tir_get_call(c->tir, tir_id);
    transform_node(c, t.f);

    for (int32_t i = 0; i < t.args.len; i++) {
        transform_node(c, t.args.ptr[i]);
    }

    vec_push(&c->mir.insts, MIR_CALL);
    push_type(c, transform_type(c, t.type));
    vec_push(&c->mir.data, t.args.len);
}

static void transform_index(Context *c, TirId tir_id) {
    TirIndex t = tir_get_index(c->tir, tir_id);
    TirId type = get_value_type(c->tir, t.a);
    transform_node(c, t.a);
    transform_node(c, t.index);
    MirTag tag = MIR_INDEX;
    if (!tir_is_reserved(remove_slice(c->tir, type), RESERVED_ERROR)) {
        tag = MIR_SLICE_INDEX;
    }
    vec_push(&c->mir.insts, tag);
    push_type(c, transform_type(c, t.type));
}

static void transform_slice(Context *c, TirId tir_id) {
    TirSlice t = tir_get_slice(c->tir, tir_id);

    vec_push(&c->mir.insts, MIR_ALLOC);
    push_type(c, transform_type(c, t.type));

    transform_node(c, t.a);
    transform_node(c, t.low);
    transform_node(c, t.high);

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
    TirId operand_type = get_value_type(c->tir, t.a);
    MirTag tag = MIR_INDEX;
    if (!tir_is_reserved(remove_slice(c->tir, operand_type), RESERVED_ERROR)) {
        tag = MIR_SLICE_INDEX;
    }
    vec_push(&c->mir.insts, tag);
    push_type(c, transform_type(c, remove_c_pointer_like(c->tir, operand_type)));
    vec_push(&c->mir.insts, MIR_ADDRESS);
    vec_push(&c->mir.insts, MIR_ASSIGN);

    vec_push(&c->mir.insts, MIR_STACK_POP);
    vec_push(&c->mir.insts, MIR_STACK_POP);
    vec_push(&c->mir.insts, MIR_STACK_POP);
}

static void transform_array_to_slice(Context *c, TirId tir_id) {
    TirCast t = tir_get_cast(c->tir, tir_id);
    TirId array_type = remove_any_pointer(c->tir, get_value_type(c->tir, t.a));
    TirId index_type = tir_get_array_type(c->tir, array_type).index;
    int64_t length = tir_get_array_length_type(c->tir, index_type).length;

    vec_push(&c->mir.insts, MIR_ALLOC);
    push_type(c, transform_type(c, t.type));

    vec_push(&c->mir.insts, MIR_STACK_COPY);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 0);
    vec_push(&c->mir.insts, MIR_I64);
    store_i64(vec_grow(&c->mir.data, 2), length);
    vec_push(&c->mir.insts, MIR_ASSIGN);

    vec_push(&c->mir.insts, MIR_STACK_COPY);
    vec_push(&c->mir.insts, MIR_ACCESS);
    vec_push(&c->mir.data, 1);
    transform_node(c, t.a);
    vec_push(&c->mir.insts, MIR_ASSIGN);
}

static void transform_new_struct(Context *c, TirId tir_id) {
    TirNewStruct t = tir_get_new_struct(c->tir, tir_id);
    vec_push(&c->mir.insts, MIR_ALLOC);
    push_type(c, transform_type(c, t.type));

    for (int32_t i = 0; i < t.fields.len; i++) {
        vec_push(&c->mir.insts, MIR_STACK_COPY);
        vec_push(&c->mir.insts, MIR_ACCESS);
        vec_push(&c->mir.data, t.field_indices.ptr[i]);
        transform_node(c, t.fields.ptr[i]);
        vec_push(&c->mir.insts, MIR_ASSIGN);
    }
}

static void transform_new_array(Context *c, TirId tir_id) {
    TirNewArray t = tir_get_new_array(c->tir, tir_id);

    vec_push(&c->mir.insts, MIR_ALLOC);
    push_type(c, transform_type(c, t.type));
    MirTypeId elem_type = transform_type(c, remove_c_pointer_like(c->tir, t.type));

    for (int32_t i = 0; i < t.args.len; i++) {
        vec_push(&c->mir.insts, MIR_STACK_COPY);
        vec_push(&c->mir.insts, MIR_I32);
        vec_push(&c->mir.data, i);
        vec_push(&c->mir.insts, MIR_INDEX);
        push_type(c, elem_type);
        transform_node(c, t.args.ptr[i]);
        vec_push(&c->mir.insts, MIR_ASSIGN);
    }
}

static bool last_is_terminator(Context *c, int32_t last_br) {
    return c->mir.insts.len - 1 > last_br && is_mir_terminator(c->mir.insts.ptr[c->mir.insts.len - 1]);
}

static void transform_statement(Context *c, TirId tir_id) {
    transform_node(c, tir_id);
    TirId type = get_value_type(c->tir, tir_id);

    if (!tir_is_reserved(type, RESERVED_ERROR) && !tir_is_reserved(type, TYPE_VOID)) {
        vec_push(&c->mir.insts, MIR_STACK_POP);
    }
}

static void transform_block(Context *c, TirId tir_id) {
    TirBlock t = tir_get_block(c->tir, tir_id);

    for (int32_t i = 0; i < t.stmts.len; i++) {
        if (i == t.stmts.len - 1) {
            transform_node(c, t.stmts.ptr[i]);
        } else {
            transform_statement(c, t.stmts.ptr[i]);
        }
    }
}

static void transform_if(Context *c, TirId tir_id) {
    TirIf t = tir_get_if(c->tir, tir_id);
    transform_node(c, t.condition);
    int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);

    for (int32_t i = 0; i < t.true_block.len; i++) {
        transform_statement(c, t.true_block.ptr[i]);
    }

    int32_t true_br = condition_br;

    if (!last_is_terminator(c, condition_br)) {
        true_br = add_br_instruction(c, MIR_BR);
    }

    int32_t false_basic_block = c->basic_block;

    for (int32_t i = 0; i < t.false_block.len; i++) {
        transform_statement(c, t.false_block.ptr[i]);
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
    TirSwitch t = tir_get_switch(c->tir, tir_id);

    if (!tir_is_reserved(t.type, TYPE_VOID)) {
        vec_push(&c->mir.insts, MIR_ALLOC);
        push_type(c, transform_type(c, t.type));
    }

    int32_t copy_inst = -1;
    bool condition_is_true = tir_is_reserved(get_value_type(c->tir, t.condition), TYPE_bool)
        && get_tir_tag(c->tir, t.condition) == TIR_INT
        && tir_get_int(c->tir, t.condition).value;

    if (!condition_is_true) {
        copy_inst = -2;
        transform_node(c, t.condition);
    }

    int32_t *br_list = arena_alloc(&c->scratch, int32_t, t.branches.len / 2);
    int32_t real_count = 0;

    for (int32_t i = 0; i < t.branches.len; i += 2) {
        TirId pattern = t.branches.ptr[i];
        TirId value = t.branches.ptr[i + 1];

        if (!tir_is_reserved(pattern, RESERVED_ERROR)) {
            if (!condition_is_true) {
                vec_push(&c->mir.insts, MIR_STACK_COPY);
                transform_node(c, pattern);
                vec_push(&c->mir.insts, MIR_EQ);
            } else {
                transform_node(c, pattern);
            }

            int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);
            if (!tir_is_reserved(t.type, TYPE_VOID)) {
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
            if (!tir_is_reserved(t.type, TYPE_VOID)) {
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

    if (!condition_is_true) {
        vec_push(&c->mir.insts, MIR_STACK_POP);
    }
}

static void transform_loop(Context *c, TirId tir_id) {
    TirLoop t = tir_get_loop(c->tir, tir_id);

    if (!tir_is_reserved(t.init, RESERVED_ERROR)) {
        transform_statement(c, t.init);
    }

    int32_t entry_br = add_br_instruction(c, MIR_BR);
    int32_t condition_basic_block = c->basic_block;
    patch_br(c, entry_br, condition_basic_block);

    transform_node(c, t.condition);
    int32_t condition_br = add_br_instruction(c, MIR_BR_IF_NOT);

    int32_t break_index = c->break_instructions.len;
    int32_t continue_index = c->continue_instructions.len;

    for (int32_t i = 0; i < t.block.len; i++) {
        transform_statement(c, t.block.ptr[i]);
    }

    int32_t continue_basic_block = condition_basic_block;

    if (!tir_is_reserved(t.next, RESERVED_ERROR)) {
        int32_t continue_br = add_br_instruction(c, MIR_BR);
        continue_basic_block = c->basic_block;
        patch_br(c, continue_br, continue_basic_block);
        transform_statement(c, t.next);
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
    TirReturn t = tir_get_return(c->tir, tir_id);

    if (!tir_is_reserved(t.value, RESERVED_ERROR)) {
        transform_node(c, t.value);
        c->basic_block++;
        vec_push(&c->mir.insts, MIR_RET);
        return;
    }

    c->basic_block++;
    vec_push(&c->mir.insts, MIR_RET_VOID);
}

static void transform_function(Context *c, int32_t block, int32_t block_length, TirId value) {
    TirId type = get_value_type(c->tir, value);
    TirFunctionType func_type = tir_get_function_type(c->tir, type);
    int32_t start = c->mir.insts.len;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir.thread, block + i)};
        transform_statement(c, statement);
    }

    if (tir_is_reserved(func_type.ret, TYPE_VOID) && (c->mir.insts.len == start || c->mir.insts.ptr[c->mir.insts.len - 1] != MIR_RET_VOID)) {
        vec_push(&c->mir.insts, MIR_RET_VOID);
    }
}

static void transform_node(Context *c, TirId tir_id) {
    switch (get_tir_tag(c->tir, tir_id)) {
        case TIR_FUNCTION: {
            TirFunction t = tir_get_function(c->tir, tir_id);
            vec_push(&c->mir.insts, MIR_GLOBAL_FUNCTION);
            push_type(c, transform_type(c, t.type));
            char const *s = tir_get_str(c->tir, t.name);
            int64_t x = (int64_t) (intptr_t) s;
            store_i64(vec_grow(&c->mir.data, 2), x);
            break;
        }
        case TIR_EXTERN_FUNCTION: {
            TirExternFunction t = tir_get_extern_function(c->tir, tir_id);
            vec_push(&c->mir.insts, MIR_GLOBAL_FUNCTION);
            push_type(c, transform_type(c, t.type));
            char const *s = tir_get_str(c->tir, t.name);
            int64_t x = (int64_t) (intptr_t) s;
            store_i64(vec_grow(&c->mir.data, 2), x);
            break;
        }
        case TIR_EXTERN_VAR: {
            TirExternVar t = tir_get_extern_var(c->tir, tir_id);
            vec_push(&c->mir.insts, MIR_GLOBAL_VAR);
            push_type(c, transform_type(c, t.type));
            char const *s = tir_get_str(c->tir, t.name);
            int64_t x = (int64_t) (intptr_t) s;
            store_i64(vec_grow(&c->mir.data, 2), x);
            break;
        }
        case TIR_INT: {
            TirInt i = tir_get_int(c->tir, tir_id);
            switch (transform_type(c, i.type).private_field_id) {
                case MIR_TYPE_BOOL: {
                    vec_push(&c->mir.insts, i.value ? MIR_TRUE : MIR_FALSE);
                    break;
                }
                case MIR_TYPE_I8: {
                    vec_push(&c->mir.insts, MIR_I8);
                    vec_push(&c->mir.data, i.value);
                    break;
                }
                case MIR_TYPE_I16: {
                    vec_push(&c->mir.insts, MIR_I16);
                    vec_push(&c->mir.data, i.value);
                    break;
                }
                case MIR_TYPE_I32: {
                    vec_push(&c->mir.insts, MIR_I32);
                    vec_push(&c->mir.data, i.value);
                    break;
                }
                case MIR_TYPE_I64: {
                    vec_push(&c->mir.insts, MIR_I64);
                    store_i64(vec_grow(&c->mir.data, 2), i.value);
                    break;
                }
                default: {
                    abort();
                }
            }
            break;
        }
        case TIR_FLOAT: {
            TirFloat f = tir_get_float(c->tir, tir_id);
            switch (transform_type(c, f.type).private_field_id) {
                case MIR_TYPE_F32: {
                    float value = (float) f.value;
                    vec_push(&c->mir.insts, MIR_F32);
                    memcpy(vec_grow(&c->mir.data, 1), &value, 4);
                    break;
                }
                case MIR_TYPE_F64: {
                    vec_push(&c->mir.insts, MIR_F64);
                    store_f64(vec_grow(&c->mir.data, 2), f.value);
                    break;
                }
            }
            break;
        }
        case TIR_NULL: {
            vec_push(&c->mir.insts, MIR_NULL);
            break;
        }
        case TIR_STRING: {
            TirString t = tir_get_string(c->tir, tir_id);
            char const *s = tir_get_str(c->tir, t.value);
            vec_push(&c->mir.insts, MIR_STRING);
            push_type(c, transform_type(c, t.type));
            int64_t x = (int64_t) (intptr_t) s;
            store_i64(vec_grow(&c->mir.data, 2), x);
            break;
        }
        case TIR_PARAMETER: {
            vec_push(&c->mir.insts, MIR_PARAMETER);
            push_type(c, transform_type(c, tir_get_variable(c->tir, tir_id).type));
            vec_push(&c->mir.data, tir_get_variable(c->tir, tir_id).index);
            break;
        }
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            vec_push(&c->mir.insts, MIR_VARIABLE);
            push_type(c, transform_type(c, tir_get_variable(c->tir, tir_id).type));
            vec_push(&c->mir.data, tir_get_variable(c->tir, tir_id).index);
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
        case TIR_INARROW: transform_cast(c, tir_id, MIR_INARROW); break;
        case TIR_SEXT: transform_cast(c, tir_id, MIR_SEXT); break;
        case TIR_ZEXT: transform_cast(c, tir_id, MIR_ZEXT); break;
        case TIR_FTOI: transform_cast(c, tir_id, MIR_FTOI); break;
        case TIR_FTRUNC: transform_cast(c, tir_id, MIR_FTRUNC); break;
        case TIR_FEXT: transform_cast(c, tir_id, MIR_FEXT); break;
        case TIR_NOP: transform_nop(c, tir_id); break;
        case TIR_ARRAY_TO_SLICE: transform_array_to_slice(c, tir_id); break;
        case TIR_CALL: transform_call(c, tir_id); break;
        case TIR_INDEX: transform_index(c, tir_id); break;
        case TIR_SLICE: transform_slice(c, tir_id); break;
        case TIR_NEW_STRUCT: transform_new_struct(c, tir_id); break;
        case TIR_NEW_ARRAY: transform_new_array(c, tir_id); break;
        case TIR_BLOCK: transform_block(c, tir_id); break;
        case TIR_IF: transform_if(c, tir_id); break;
        case TIR_SWITCH: transform_switch(c, tir_id); break;
        case TIR_LOOP: transform_loop(c, tir_id); break;
        case TIR_BREAK: transform_break(c); break;
        case TIR_CONTINUE: transform_continue(c); break;
        case TIR_RETURN: transform_return(c, tir_id); break;
        default: compiler_error("tir_to_mir: unimplemented tag");
    }
}

static void tir_deps_to_mir(Context *c) {
    for (int32_t i = 0; i < c->tir.thread->structs.len; i++) {
        transform_type(c, c->tir.thread->structs.ptr[i]);
    }
    for (int32_t i = 0; i < c->tir.thread->extern_functions.len; i++) {
        TirExternFunction f = tir_get_extern_function(c->tir, c->tir.thread->extern_functions.ptr[i]);
        MirTypeId type = transform_type(c, f.type);
        vec_push(&c->mir.extern_functions, (MirGlobal) {
            .name = tir_get_str(c->tir, f.name),
            .type = type,
        });
    }
    for (int32_t i = 0; i < c->tir.thread->extern_vars.len; i++) {
        TirExternVar v = tir_get_extern_var(c->tir, c->tir.thread->extern_vars.ptr[i]);
        vec_push(&c->mir.extern_vars, (MirGlobal) {
            .name = tir_get_str(c->tir, v.name),
            .type = transform_type(c, v.type),
        });
    }
}

Mir tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch) {
    Mir mir = {0};
    mir.ends = arena_alloc(permanent, int32_t, input->function_count + 1);
    mir.data_starts = arena_alloc(permanent, int32_t, input->function_count);
    mir.main_function = -1;
    MirTypeId *global_struct_types = arena_alloc(&scratch, MirTypeId, input->global_deps->terms.terms.len);

    {
        Context c = {0};
        c.mir = mir;
        c.target = input->target;
        c.tir.global = input->global_deps;
        c.tir.thread = input->global_deps;
        c.scratch = scratch;
        c.global_struct_types = global_struct_types;
        tir_deps_to_mir(&c);
        mir = c.mir;
    }

    for (int32_t i = 0; i < input->function_count; i++) {
        Context c = {0};
        c.mir = mir;
        c.target = input->target;
        c.tir.global = input->global_deps;
        c.tir.thread = &input->insts[i].deps;
        c.scratch = scratch;
        c.global_struct_types = global_struct_types;
        MirTypeId *local_struct_types = arena_alloc(&scratch, MirTypeId, c.tir.thread->terms.terms.len);
        c.local_struct_types = local_struct_types;
        mir.ends[i] = c.mir.insts.len;
        mir.data_starts[i] = c.mir.data.len;
        tir_deps_to_mir(&c);
        transform_function(&c, input->insts[i].body_first, input->insts[i].body_length, input->functions[i]);
        free(c.break_instructions.ptr);
        free(c.continue_instructions.ptr);
        TirFunction f = tir_get_function(c.tir, input->functions[i]);
        MirTypeId type = transform_type(&c, f.type);
        vec_push(&c.mir.functions, (MirGlobal) {
            .name = tir_get_str(c.tir, f.name),
            .type = type,
        });
        mir = c.mir;
        mir.ends[i + 1] = c.mir.insts.len;
        if (tir_eq(input->global_deps->main, input->functions[i])) {
            mir.main_function = i;
        }
    }

    int32_t data_count = 0;
    for (int32_t i = 0; i < mir.insts.len; i++) {
        switch (mir.insts.ptr[i]) {
            #define DATA(name, type) + (int32_t) (sizeof(type) / sizeof(int32_t))
            #define X(name, ...) case MIR_##name: { data_count += (0 __VA_ARGS__); break; }
            #include "mir-defs"
        }
    }
    if (data_count != mir.data.len) {
        abort();
    }

    return mir;
}
