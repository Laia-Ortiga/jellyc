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
    TirContext tir;

    VariableState *var_states;
    AstId *var_refs;
    int32_t var_count;
    int32_t var_states_top;
    int32_t var_states_loop_top;
    Arena scratch;
    int error;
} LinearChecker;

static void error(LinearChecker *c, AstId node, ErrorKind kind) {
    SourceIndex token = get_ast_token(node, &c->asts[c->file]);
    SourceLoc loc = {0};
    loc.path = c->paths[c->file];
    loc.source = c->sources[c->file];
    loc.where = token;
    loc.len = 1;
    loc.mark = token;
    print_diagnostic(&loc, &(Diagnostic) {.kind = kind});
    c->error = 1;
}

static void check_node(LinearChecker *c, TirId node, ExpectedValue expected_category);

static void check_value(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    if (expected_category == RVALUE && !type_is_affine(c->tir, get_value_type(c->tir, node))) {
        return;
    }

    check_node(c, node, expected_category);
}

static void check_let(LinearChecker *c, TirId node) {
    TirId v = {get_term_data(c->tir, node)->b};
    int32_t var = get_term_data(c->tir, v)->b;
    c->var_states[var] = VAR_NOT_CONSUMED;
    c->var_refs[var] = get_term_data(c->tir, node)->node;
    c->var_states_top++;
    TirId init = {get_term_data(c->tir, node)->c};
    check_value(c, init, RVALUE);
}

static void check_unary_arit(LinearChecker *c, TirId node) {
    TirId operand = {get_term_data(c->tir, node)->b};
    check_value(c, operand, RVALUE);
}

static void check_address(LinearChecker *c, TirId node) {
    TirId operand = {get_term_data(c->tir, node)->b};
    check_value(c, operand, STATEMENT);
}

static void check_deref(LinearChecker *c, TirId node) {
    TirId operand = {get_term_data(c->tir, node)->b};
    check_value(c, operand, RVALUE);
}

static void check_binary_arit(LinearChecker *c, TirId node) {
    TirId left = {get_term_data(c->tir, node)->b};
    TirId right = {get_term_data(c->tir, node)->c};
    check_value(c, left, RVALUE);
    check_value(c, right, RVALUE);
}

static void check_assign(LinearChecker *c, TirId node) {
    TirId left = {get_term_data(c->tir, node)->b};
    TirId right = {get_term_data(c->tir, node)->c};
    TirId type = get_value_type(c->tir, left);
    if (type_is_affine(c->tir, type)) {
        AstId ast_id = get_term_data(c->tir, node)->node;
        error(c, ast_id, ERROR_AFFINE_ASSIGNMENT);
    }
    check_value(c, right, RVALUE);
    check_value(c, left, RVALUE);
}

static void check_access(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TirId operand = {get_term_data(c->tir, node)->b};
    int32_t index = get_term_data(c->tir, node)->c;
    TirId type = get_value_type(c->tir, operand);
    TirId result_type = get_struct_type_field(c->tir, type, index);
    if (expected_category == RVALUE && !type_is_affine(c->tir, result_type)) {
        // If you are accessing a field that is not affine, no need to consume it.
        check_value(c, operand, LVALUE);
    } else {
        check_value(c, operand, expected_category);
    }
}

static void check_call(LinearChecker *c, TirId node) {
    TirId operand = {get_term_data(c->tir, node)->b};
    int32_t args = get_term_data(c->tir, node)->c;
    TirId type = get_value_type(c->tir, operand);
    int32_t arg_count = get_function_type(c->tir, type).param_count;
    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        check_value(c, arg, RVALUE);
    }
    check_value(c, operand, RVALUE);
}

static void check_index(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TirId left = {get_term_data(c->tir, node)->b};
    TirId right = {get_term_data(c->tir, node)->c};
    check_value(c, right, RVALUE);
    check_value(c, left, expected_category);
}

static void check_slice(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TermData const *data = get_term_data(c->tir, node);
    TirId operand = {data->b};
    int32_t index = data->c;
    TirId low = {get_term_extra(c->tir, index)};
    TirId high = {get_term_extra(c->tir, index + 1)};
    check_value(c, low, RVALUE);
    check_value(c, high, RVALUE);
    check_value(c, operand, expected_category);
}

static void check_new_type(LinearChecker *c, TirId node) {
    int32_t args = get_term_data(c->tir, node)->b;
    int32_t arg_count = get_term_data(c->tir, node)->c;
    for (int32_t i = 0; i < arg_count; i++) {
        TirId arg = {get_term_extra(c->tir, args + i)};
        check_value(c, arg, RVALUE);
    }
}

static void check_return(LinearChecker *c, TirId node) {
    TirId operand = {get_term_data(c->tir, node)->b};
    if (operand.id) {
        check_value(c, operand, RVALUE);
    }
}

static VariableState *copy_state(LinearChecker *c) {
    VariableState *state = arena_alloc(&c->scratch, VariableState, c->var_count);
    memcpy(state, c->var_states, c->var_states_top * sizeof(VariableState));
    return state;
}

static void check_if(LinearChecker *c, TirId node) {
    TirId condition = {get_term_data(c->tir, node)->b};
    int32_t extra = get_term_data(c->tir, node)->c;
    int32_t true_block = get_term_extra(c->tir, extra);
    int32_t true_block_length = get_term_extra(c->tir, extra + 1);
    int32_t false_block = get_term_extra(c->tir, extra + 2);
    int32_t false_block_length = get_term_extra(c->tir, extra + 3);
    check_value(c, condition, RVALUE);

    LinearChecker true_ctx = *c;
    VariableState *enter_state = copy_state(&true_ctx);

    for (int32_t i = 0; i < true_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, true_block + i)};
        check_node(&true_ctx, statement, STATEMENT);
    }

    LinearChecker false_ctx = true_ctx;
    false_ctx.var_states = enter_state;

    for (int32_t i = 0; i < false_block_length; i++) {
        TirId statement = {get_term_extra(c->tir, false_block + i)};
        check_node(&false_ctx, statement, STATEMENT);
    }

    for (int32_t i = 0; i < c->var_states_top; i++) {
        if (c->var_states[i] != false_ctx.var_states[i]) {
            c->var_states[i] = VAR_CONSUMED;
        }
    }
}

static void check_switch(LinearChecker *c, TirId node) {
    TirId switch_ = {get_term_data(c->tir, node)->b};
    int32_t extra = get_term_data(c->tir, node)->c;
    int32_t branches = get_term_extra(c->tir, extra);
    int32_t branch_count = get_term_extra(c->tir, extra + 1);
    check_value(c, switch_, RVALUE);

    // First, check all patterns
    for (int32_t i = 0; i < branch_count; i++) {
        TirId pattern = {get_term_extra(c->tir, branches + i * 2)};

        if (pattern.id) {
            check_value(c, pattern, RVALUE);
        }
    }

    LinearChecker first_pattern_ctx = *c;
    first_pattern_ctx.var_states = copy_state(&first_pattern_ctx);

    // Then, check each branch for discrepancies
    for (int32_t j = 0; j < branch_count; j++) {
        TirId value = {get_term_extra(c->tir, branches + j * 2 + 1)};

        if (j != 0) {
            LinearChecker pattern_ctx = *c;
            pattern_ctx.var_states = copy_state(&pattern_ctx);
            check_value(&pattern_ctx, value, RVALUE);

            for (int32_t i = 0; i < c->var_states_top; i++) {
                if (first_pattern_ctx.var_states[i] != pattern_ctx.var_states[i]
                    || first_pattern_ctx.var_states[i] == VAR_CONSUMED) {
                    c->var_states[i] = VAR_CONSUMED;
                }
            }
        } else {
            check_value(&first_pattern_ctx, value, RVALUE);
        }
    }
}

static void check_loop(LinearChecker *c, TirId node) {
    TirId condition = {get_term_data(c->tir, node)->b};
    int32_t extra = get_term_data(c->tir, node)->c;
    TirId let = {get_term_extra(c->tir, extra)};
    if (let.id) {
        check_value(c, let, STATEMENT);
    }
    TirId next = {get_term_extra(c->tir, extra + 1)};
    int32_t block = get_term_extra(c->tir, extra + 2);
    int32_t block_length = get_term_extra(c->tir, extra + 3);

    check_value(c, condition, RVALUE);
    int32_t prev_loop_top = c->var_states_loop_top;
    c->var_states_loop_top = c->var_states_top;

    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        check_node(c, statement, STATEMENT);
    }

    if (next.id) {
        check_value(c, next, STATEMENT);
    }

    c->var_states_loop_top = prev_loop_top;
}

static void check_function(
    LinearChecker *c,
    SubstructuralAnalysisInput *input,
    int32_t f_index
) {
    int32_t block = input->insts[f_index].body_first;
    int32_t block_length = input->insts[f_index].body_length;
    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_term_extra(c->tir, block + i)};
        check_node(c, statement, STATEMENT);
    }
}

static void check_node(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    switch (get_term_tag(c->tir, node)) {
        default: {
            abort();
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL:
        case TIR_STRING: {
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            int32_t var = get_term_data(c->tir, node)->b;
            AstId ast_id = c->var_refs[var];
            switch (c->var_states[var]) {
                case VAR_CONSUMED: {
                    error(c, ast_id, ERROR_CONSUMED_VALUE_USED);
                    return;
                }
                case VAR_NOT_CONSUMED: {
                    switch (expected_category) {
                        case RVALUE: {
                            c->var_states[var] = VAR_CONSUMED;
                            if (var < c->var_states_loop_top) {
                                error(c, ast_id, ERROR_CONSUMED_IN_LOOP);
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
                        case RVALUE: error(c, ast_id, ERROR_MOVE_BORROWED); break;
                        case LVALUE: /* Allow multiple constant borrows. */ break;
                        case LVALUE_MUT: error(c, ast_id, ERROR_BORROWED_MUTABLE_SHARED); break;
                        case STATEMENT: abort();
                    }
                    return;
                }
                case VAR_BORROWED_MUT: {
                    switch (expected_category) {
                        case RVALUE: error(c, ast_id, ERROR_MOVE_BORROWED); break;
                        case LVALUE: error(c, ast_id, ERROR_BORROWED_MUTABLE_SHARED); break;
                        case LVALUE_MUT: error(c, ast_id, ERROR_MULTIBLE_MUTABLE_BORROWS); break;
                        case STATEMENT: abort();
                    }
                    return;
                }
            }
            break;
        }
        case TIR_LET: {
            check_let(c, node);
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
            check_unary_arit(c, node);
            break;
        }
        case TIR_ADDRESS_OF_TEMPORARY:
        case TIR_ADDRESS: {
            check_address(c, node);
            break;
        }
        case TIR_DEREF: {
            check_deref(c, node);
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
            check_binary_arit(c, node);
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
            check_assign(c, node);
            break;
        }
        case TIR_ACCESS: {
            check_access(c, node, expected_category);
            break;
        }
        case TIR_CALL: {
            check_call(c, node);
            break;
        }
        case TIR_INDEX: {
            check_index(c, node, expected_category);
            break;
        }
        case TIR_SLICE: {
            check_slice(c, node, expected_category);
            break;
        }
        case TIR_NEW_STRUCT:
        case TIR_NEW_ARRAY: {
            check_new_type(c, node);
            break;
        }
        case TIR_IF: {
            check_if(c, node);
            break;
        }
        case TIR_SWITCH: {
            check_switch(c, node);
            break;
        }
        case TIR_LOOP: {
            check_loop(c, node);
            break;
        }
        case TIR_BREAK:
        case TIR_CONTINUE: {
            break;
        }
        case TIR_RETURN: {
            check_return(c, node);
            break;
        }
    }
}

int check_substructural_types(SubstructuralAnalysisInput *input, Arena scratch) {
    int error = 0;
    for (int32_t i = 0; i < input->function_count; i++) {
        LinearChecker c = {0};
        c.paths = input->paths;
        c.sources = input->sources;
        c.asts = input->asts;
        c.ast_refs = input->ast_refs;
        c.file = input->ast_refs[input->functions[i].id].file;
        c.tir.global = input->global_deps;
        c.tir.thread = &input->insts[i].deps;
        c.scratch = scratch;
        c.var_count = input->insts[i].local_count;
        c.var_states = arena_alloc(&c.scratch, VariableState, c.var_count);
        c.var_refs = arena_alloc(&c.scratch, AstId, c.var_count);
        check_function(&c, input, i);
        if (c.error) {
            error = 1;
        }
    }
    return error;
}
