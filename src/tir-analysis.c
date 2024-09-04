#include "tir-analysis.h"

#include "arena.h"
#include "data/ast.h"
#include "diagnostic.h"
#include "fwd.h"
#include "lex.h"
#include "data/tir.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    RVALUE,
    LVALUE,
    LVALUE_MUT,
    STATEMENT,
} ExpectedValue;

typedef enum {
    VAR_NOT_CONSUMED,
    VAR_CONSUMED,
    VAR_BORROWED,
    VAR_BORROWED_MUT,
} VariableState;

typedef struct {
    char **paths;
    String *sources;
    Ast *asts;
    AstRef *ast_refs;

    int32_t file;
    TirContext tir_ctx;
    TirInstList *tir_insts;

    VariableState *var_states;
    AstId *var_refs;
    int32_t var_count;
    int32_t var_states_top;
    int32_t var_states_loop_top;
    Arena scratch;
    int error;
} LinearChecker;

static void error(LinearChecker *ctx, AstId node, ErrorKind kind) {
    SourceIndex token = get_ast_token(node, &ctx->asts[ctx->file]);
    SourceLoc loc = {0};
    loc.path = ctx->paths[ctx->file];
    loc.source = ctx->sources[ctx->file];
    loc.where = token;
    loc.len = 1;
    loc.mark = token;
    print_diagnostic(&loc, &(Diagnostic) {.kind = kind});
    ctx->error = 1;
}

static void check_node(LinearChecker *ctx, TirId node, ExpectedValue expected_category);

static void check_value(LinearChecker *ctx, ValueId value, ExpectedValue expected_category) {
    if (expected_category == RVALUE && !type_is_linear(ctx->tir_ctx, get_value_type(ctx->tir_ctx, value))) {
        return;
    }

    switch (get_value_tag(ctx->tir_ctx, value)) {
        case VAL_ERROR: {
            abort();
        }
        case VAL_FUNCTION:
        case VAL_EXTERN_FUNCTION:
        case VAL_EXTERN_VAR:
        case VAL_CONST_INT:
        case VAL_CONST_FLOAT:
        case VAL_CONST_NULL:
        case VAL_STRING: {
            break;
        }
        case VAL_TEMPORARY: {
            TirId tir_id = {get_value_data(ctx->tir_ctx, value)->index};
            check_node(ctx, tir_id, expected_category);
            break;
        }
        case VAL_VARIABLE:
        case VAL_MUTABLE_VARIABLE: {
            int32_t var = get_value_data(ctx->tir_ctx, value)->index;
            AstId ast_id = ctx->var_refs[var];
            switch (ctx->var_states[var]) {
                case VAR_CONSUMED: {
                    error(ctx, ast_id, ERROR_CONSUMED_VALUE_USED);
                    return;
                }
                case VAR_NOT_CONSUMED: {
                    switch (expected_category) {
                        case RVALUE: {
                            ctx->var_states[var] = VAR_CONSUMED;
                            if (var < ctx->var_states_loop_top) {
                                error(ctx, ast_id, ERROR_CONSUMED_IN_LOOP);
                            }
                            break;
                        }
                        case LVALUE: break;
                        case LVALUE_MUT: break;
                        case STATEMENT: return;
                    }
                    return;
                }
                case VAR_BORROWED: {
                    switch (expected_category) {
                        case RVALUE: error(ctx, ast_id, ERROR_MOVE_BORROWED); break;
                        case LVALUE: /* Allow multiple constant borrows. */ break;
                        case LVALUE_MUT: error(ctx, ast_id, ERROR_BORROWED_MUTABLE_SHARED); break;
                        case STATEMENT: abort();
                    }
                    return;
                }
                case VAR_BORROWED_MUT: {
                    switch (expected_category) {
                        case RVALUE: error(ctx, ast_id, ERROR_MOVE_BORROWED); break;
                        case LVALUE: error(ctx, ast_id, ERROR_BORROWED_MUTABLE_SHARED); break;
                        case LVALUE_MUT: error(ctx, ast_id, ERROR_MULTIBLE_MUTABLE_BORROWS); break;
                        case STATEMENT: abort();
                    }
                    return;
                }
            }
            break;
        }
    }
}

static void check_let(LinearChecker *ctx, TirId node) {
    int32_t var = get_tir_data(ctx->tir_insts, node).left;
    ctx->var_states[var] = VAR_NOT_CONSUMED;
    ctx->var_refs[var] = get_tir_data(ctx->tir_insts, node).node;
    ctx->var_states_top++;
    ValueId init = {get_tir_data(ctx->tir_insts, node).right};
    check_value(ctx, init, RVALUE);
}

static void check_unary_arit(LinearChecker *ctx, TirId node) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    check_value(ctx, operand, RVALUE);
}

static void check_address(LinearChecker *ctx, TirId node) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    check_value(ctx, operand, STATEMENT);
}

static void check_deref(LinearChecker *ctx, TirId node) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    check_value(ctx, operand, RVALUE);
}

static void check_binary_arit(LinearChecker *ctx, TirId node) {
    ValueId left = {get_tir_data(ctx->tir_insts, node).left};
    ValueId right = {get_tir_data(ctx->tir_insts, node).right};
    check_value(ctx, left, RVALUE);
    check_value(ctx, right, RVALUE);
}

static void check_assign(LinearChecker *ctx, TirId node) {
    ValueId left = {get_tir_data(ctx->tir_insts, node).left};
    ValueId right = {get_tir_data(ctx->tir_insts, node).right};
    TypeId type = get_value_type(ctx->tir_ctx, left);
    if (type_is_linear(ctx->tir_ctx, type)) {
        AstId ast_id = get_tir_data(ctx->tir_insts, node).node;
        error(ctx, ast_id, ERROR_LINEAR_ASSIGNMENT);
    }
    check_value(ctx, right, RVALUE);
    check_value(ctx, left, RVALUE);
}

static void check_access(LinearChecker *ctx, TirId node, ExpectedValue expected_category) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    int32_t index = get_tir_data(ctx->tir_insts, node).right;
    TypeId type = get_value_type(ctx->tir_ctx, operand);
    TypeId result_type = get_struct_type_field(ctx->tir_ctx, type, index);
    if (expected_category == RVALUE && !type_is_linear(ctx->tir_ctx, result_type)) {
        // If you are accessing a field that is not affine, no need to consume it.
        check_value(ctx, operand, LVALUE);
    } else {
        check_value(ctx, operand, expected_category);
    }
}

static void check_call(LinearChecker *ctx, TirId node) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    int32_t args = get_tir_data(ctx->tir_insts, node).right;
    TypeId type = get_value_type(ctx->tir_ctx, operand);
    int32_t arg_count = get_function_type(ctx->tir_ctx, type).param_count;
    for (int32_t i = 0; i < arg_count; i++) {
        ValueId arg = {get_tir_extra(ctx->tir_insts, args + i)};
        check_value(ctx, arg, RVALUE);
    }
    check_value(ctx, operand, RVALUE);
}

static void check_index(LinearChecker *ctx, TirId node, ExpectedValue expected_category) {
    ValueId left = {get_tir_data(ctx->tir_insts, node).left};
    ValueId right = {get_tir_data(ctx->tir_insts, node).right};
    check_value(ctx, right, RVALUE);
    check_value(ctx, left, expected_category);
}

static void check_slice(LinearChecker *ctx, TirId node, ExpectedValue expected_category) {
    TirInstData data = get_tir_data(ctx->tir_insts, node);
    ValueId operand = {data.left};
    int32_t index = data.right;
    ValueId low = {get_tir_extra(ctx->tir_insts, index)};
    ValueId high = {get_tir_extra(ctx->tir_insts, index + 1)};
    check_value(ctx, low, RVALUE);
    check_value(ctx, high, RVALUE);
    check_value(ctx, operand, expected_category);
}

static void check_new_type(LinearChecker *ctx, TirId node) {
    int32_t args = get_tir_data(ctx->tir_insts, node).left;
    int32_t arg_count = get_tir_data(ctx->tir_insts, node).right;
    for (int32_t i = 0; i < arg_count; i++) {
        ValueId arg = {get_tir_extra(ctx->tir_insts, args + i)};
        check_value(ctx, arg, RVALUE);
    }
}

static void check_return(LinearChecker *ctx, TirId node) {
    ValueId operand = {get_tir_data(ctx->tir_insts, node).left};
    if (operand.id) {
        check_value(ctx, operand, RVALUE);
    }
}

static VariableState *copy_state(LinearChecker *ctx) {
    VariableState *state = arena_alloc(&ctx->scratch, VariableState, ctx->var_count);
    memcpy(state, ctx->var_states, ctx->var_states_top * sizeof(VariableState));
    return state;
}

static void check_if(LinearChecker *ctx, TirId node) {
    ValueId condition = {get_tir_data(ctx->tir_insts, node).left};
    int32_t extra = get_tir_data(ctx->tir_insts, node).right;
    int32_t true_block = get_tir_extra(ctx->tir_insts, extra);
    int32_t true_block_length = get_tir_extra(ctx->tir_insts, extra + 1);
    int32_t false_block = get_tir_extra(ctx->tir_insts, extra + 2);
    int32_t false_block_length = get_tir_extra(ctx->tir_insts, extra + 3);
    check_value(ctx, condition, RVALUE);

    LinearChecker true_ctx = *ctx;
    VariableState *enter_state = copy_state(&true_ctx);

    for (int32_t i = 0; i < true_block_length; i++) {
        TirId statement = {get_tir_extra(ctx->tir_insts, true_block + i)};
        check_node(&true_ctx, statement, STATEMENT);
    }

    LinearChecker false_ctx = true_ctx;
    false_ctx.var_states = enter_state;

    for (int32_t i = 0; i < false_block_length; i++) {
        TirId statement = {get_tir_extra(ctx->tir_insts, false_block + i)};
        check_node(&false_ctx, statement, STATEMENT);
    }

    for (int32_t i = 0; i < ctx->var_states_top; i++) {
        if (ctx->var_states[i] != false_ctx.var_states[i]) {
            ctx->var_states[i] = VAR_CONSUMED;
        }
    }
}

static void check_switch(LinearChecker *ctx, TirId node) {
    ValueId switch_ = {get_tir_data(ctx->tir_insts, node).left};
    int32_t extra = get_tir_data(ctx->tir_insts, node).right;
    int32_t branches = get_tir_extra(ctx->tir_insts, extra);
    int32_t branch_count = get_tir_extra(ctx->tir_insts, extra + 1);
    check_value(ctx, switch_, RVALUE);

    // First, check all patterns
    for (int32_t i = 0; i < branch_count; i++) {
        ValueId pattern = {get_tir_extra(ctx->tir_insts, branches + i * 2)};

        if (pattern.id) {
            check_value(ctx, pattern, RVALUE);
        }
    }

    LinearChecker first_pattern_ctx = *ctx;
    first_pattern_ctx.var_states = copy_state(&first_pattern_ctx);

    // Then, check each branch for discrepancies
    for (int32_t j = 0; j < branch_count; j++) {
        ValueId value = {get_tir_extra(ctx->tir_insts, branches + j * 2 + 1)};

        if (j != 0) {
            LinearChecker pattern_ctx = *ctx;
            pattern_ctx.var_states = copy_state(&pattern_ctx);
            check_value(&pattern_ctx, value, RVALUE);

            for (int32_t i = 0; i < ctx->var_states_top; i++) {
                if (first_pattern_ctx.var_states[i] != pattern_ctx.var_states[i]
                    || first_pattern_ctx.var_states[i] == VAR_CONSUMED) {
                    ctx->var_states[i] = VAR_CONSUMED;
                }
            }
        } else {
            check_value(&first_pattern_ctx, value, RVALUE);
        }
    }
}

static void check_loop(LinearChecker *ctx, TirId node) {
    ValueId condition = {get_tir_data(ctx->tir_insts, node).left};
    int32_t extra = get_tir_data(ctx->tir_insts, node).right;
    ValueId next = {get_tir_extra(ctx->tir_insts, extra)};
    int32_t block = get_tir_extra(ctx->tir_insts, extra + 1);
    int32_t block_length = get_tir_extra(ctx->tir_insts, extra + 2);

    check_value(ctx, condition, RVALUE);
    int32_t prev_loop_top = ctx->var_states_loop_top;
    ctx->var_states_loop_top = ctx->var_states_top;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_tir_extra(ctx->tir_insts, block + i)};
        check_node(ctx, statement, STATEMENT);
    }

    if (next.id) {
        check_value(ctx, next, STATEMENT);
    }

    ctx->var_states_loop_top = prev_loop_top;
}

static void check_function(LinearChecker *ctx, TirId node) {
    int32_t block = get_tir_data(ctx->tir_insts, node).left;
    int32_t block_length = get_tir_data(ctx->tir_insts, node).right;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_tir_extra(ctx->tir_insts, block + i)};
        check_node(ctx, statement, STATEMENT);
    }
}

static void check_node(LinearChecker *ctx, TirId node, ExpectedValue expected_category) {
    switch (get_tir_tag(ctx->tir_insts, node)) {
        case TIR_FUNCTION: {
            abort();
        }
        case TIR_LET:
        case TIR_MUT: {
            check_let(ctx, node);
            break;
        }
        case TIR_VALUE: {
            ValueId value = {get_tir_data(ctx->tir_insts, node).left};
            check_value(ctx, value, expected_category);
            break;
        }
        case TIR_PLUS:
        case TIR_MINUS:
        case TIR_NOT:
        case TIR_ITOF:
        case TIR_ITRUNC:
        case TIR_SEXT:
        case TIR_ZEXT:
        case TIR_FTOI:
        case TIR_FTRUNC:
        case TIR_FEXT:
        case TIR_PTR_CAST:
        case TIR_NOP:
        case TIR_ARRAY_TO_SLICE: {
            check_unary_arit(ctx, node);
            break;
        }
        case TIR_ADDRESS_OF_TEMPORARY:
        case TIR_ADDRESS: {
            check_address(ctx, node);
            break;
        }
        case TIR_DEREF: {
            check_deref(ctx, node);
            break;
        }
        case TIR_ADD:
        case TIR_SUB:
        case TIR_MUL:
        case TIR_DIV:
        case TIR_MOD:
        case TIR_AND:
        case TIR_OR:
        case TIR_XOR:
        case TIR_SHL:
        case TIR_SHR:
        case TIR_EQ:
        case TIR_NE:
        case TIR_LT:
        case TIR_GT:
        case TIR_LE:
        case TIR_GE: {
            check_binary_arit(ctx, node);
            break;
        }
        case TIR_ASSIGN:
        case TIR_ASSIGN_ADD:
        case TIR_ASSIGN_SUB:
        case TIR_ASSIGN_MUL:
        case TIR_ASSIGN_DIV:
        case TIR_ASSIGN_MOD:
        case TIR_ASSIGN_AND:
        case TIR_ASSIGN_OR:
        case TIR_ASSIGN_XOR: {
            check_assign(ctx, node);
            break;
        }
        case TIR_ACCESS: {
            check_access(ctx, node, expected_category);
            break;
        }
        case TIR_CALL: {
            check_call(ctx, node);
            break;
        }
        case TIR_INDEX: {
            check_index(ctx, node, expected_category);
            break;
        }
        case TIR_SLICE: {
            check_slice(ctx, node, expected_category);
            break;
        }
        case TIR_NEW_STRUCT:
        case TIR_NEW_ARRAY: {
            check_new_type(ctx, node);
            break;
        }
        case TIR_IF: {
            check_if(ctx, node);
            break;
        }
        case TIR_SWITCH: {
            check_switch(ctx, node);
            break;
        }
        case TIR_LOOP: {
            check_loop(ctx, node);
            break;
        }
        case TIR_BREAK:
        case TIR_CONTINUE: {
            break;
        }
        case TIR_RETURN: {
            check_return(ctx, node);
            break;
        }
    }
}

int check_substructural_types(SubstructuralAnalysisInput *input, Arena scratch) {
    int error = 0;
    for (int32_t i = 0; i < input->function_count; i++) {
        LinearChecker ctx = {0};
        ctx.paths = input->paths;
        ctx.sources = input->sources;
        ctx.asts = input->asts;
        ctx.ast_refs = input->ast_refs;
        ctx.file = input->ast_refs[input->functions[i].id].file;
        ctx.tir_ctx.global = input->global_deps;
        ctx.tir_ctx.thread = &input->insts[i];
        ctx.tir_insts = &input->insts[i].insts;
        ctx.scratch = scratch;
        ctx.var_count = input->insts[i].local_count;
        ctx.var_states = arena_alloc(&ctx.scratch, VariableState, ctx.var_count);
        ctx.var_refs = arena_alloc(&ctx.scratch, AstId, ctx.var_count);
        check_function(&ctx, input->insts[i].first);
        if (ctx.error) {
            error = 1;
        }
    }
    return error;
}
