#include "tir-analysis.h"

#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "fwd.h"
#include "lex.h"
#include "tir.h"
#include "type.h"

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
    Paths paths;
    Sources sources;
    Asts asts;
    AstRefs ast_refs;

    FileId file;
    TirContext tir;

    VariableState *var_states;
    AstId *var_refs;
    int32_t var_count;
    int32_t var_states_top;
    int32_t var_states_loop_top;
    Arena scratch;
    int error;
} LinearChecker;

static void error(LinearChecker *c, AstId node, Diagnostic d) {
    SourceIndex token = get_ast_token(&nth(c->asts, c->file), node);
    SourceLoc loc = {0};
    loc.path = nth(c->paths, c->file);
    loc.source = nth(c->sources, c->file);
    loc.where = token;
    loc.len = 1;
    loc.mark = token;
    print_diagnostic(&loc, &d);
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
    TirLet t = tir_get_let(c->tir, node);
    int32_t var = tir_get_variable(c->tir, t.var).index;
    c->var_states[var] = VAR_NOT_CONSUMED;
    c->var_refs[var] = t.node;
    c->var_states_top++;
    check_value(c, t.init, RVALUE);
}

static void check_unary_arit(LinearChecker *c, TirId node) {
    TirUnary t = tir_get_unary(c->tir, node);
    check_value(c, t.a, RVALUE);
}

static void check_cast(LinearChecker *c, TirId node) {
    TirCast t = tir_get_cast(c->tir, node);
    check_value(c, t.a, RVALUE);
}

static void check_address(LinearChecker *c, TirId node) {
    TirUnary t = tir_get_unary(c->tir, node);
    check_value(c, t.a, STATEMENT);
}

static void check_deref(LinearChecker *c, TirId node) {
    TirUnary t = tir_get_unary(c->tir, node);
    check_value(c, t.a, RVALUE);
}

static void check_binary_arit(LinearChecker *c, TirId node) {
    TirBinary t = tir_get_binary(c->tir, node);
    check_value(c, t.a, RVALUE);
    check_value(c, t.b, RVALUE);
}

static void check_assign(LinearChecker *c, TirId node) {
    TirBinary t = tir_get_binary(c->tir, node);
    TirId type = get_value_type(c->tir, t.a);
    if (type_is_affine(c->tir, type)) {
        error(c, t.node, Diagnostic(ErrorAffineAssignment, {0}));
    }
    check_value(c, t.b, RVALUE);
    check_value(c, t.a, RVALUE);
}

static void check_access(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TirAccess t = tir_get_access(c->tir, node);
    if (expected_category == RVALUE && !type_is_affine(c->tir, t.type)) {
        // If you are accessing a field that is not affine, no need to consume it.
        check_value(c, t.s, LVALUE);
    } else {
        check_value(c, t.s, expected_category);
    }
}

static void check_call(LinearChecker *c, TirId node) {
    TirCall t = tir_get_call(c->tir, node);
    for (int32_t i = 0; i < t.args.len; i++) {
        check_value(c, t.args.ptr[i], RVALUE);
    }
    check_value(c, t.f, RVALUE);
}

static void check_index(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TirIndex t = tir_get_index(c->tir, node);
    check_value(c, t.index, RVALUE);
    check_value(c, t.a, expected_category);
}

static void check_slice(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    TirSlice t = tir_get_slice(c->tir, node);
    check_value(c, t.low, RVALUE);
    check_value(c, t.high, RVALUE);
    check_value(c, t.a, expected_category);
}

static void check_new_struct(LinearChecker *c, TirId node) {
    TirNewStruct t = tir_get_new_struct(c->tir, node);
    for (int32_t i = 0; i < t.fields.len; i++) {
        check_value(c, t.fields.ptr[i], RVALUE);
    }
}

static void check_new_array(LinearChecker *c, TirId node) {
    TirNewArray t = tir_get_new_array(c->tir, node);
    for (int32_t i = 0; i < t.args.len; i++) {
        check_value(c, t.args.ptr[i], RVALUE);
    }
}

static void check_return(LinearChecker *c, TirId node) {
    TirReturn t = tir_get_return(c->tir, node);
    if (t.value.id) {
        check_value(c, t.value, RVALUE);
    }
}

static VariableState *copy_state(LinearChecker *c) {
    VariableState *state = arena_alloc(&c->scratch, VariableState, c->var_count);
    memcpy(state, c->var_states, c->var_states_top * sizeof(VariableState));
    return state;
}

static void check_if(LinearChecker *c, TirId node) {
    TirIf t = tir_get_if(c->tir, node);
    check_value(c, t.condition, RVALUE);

    LinearChecker true_ctx = *c;
    VariableState *enter_state = copy_state(&true_ctx);

    for (int32_t i = 0; i < t.true_block.len; i++) {
        check_node(&true_ctx, t.true_block.ptr[i], STATEMENT);
    }

    LinearChecker false_ctx = true_ctx;
    false_ctx.var_states = enter_state;

    for (int32_t i = 0; i < t.false_block.len; i++) {
        check_node(&false_ctx, t.false_block.ptr[i], STATEMENT);
    }

    for (int32_t i = 0; i < c->var_states_top; i++) {
        if (c->var_states[i] != false_ctx.var_states[i]) {
            c->var_states[i] = VAR_CONSUMED;
        }
    }
}

static void check_switch(LinearChecker *c, TirId node) {
    TirSwitch t = tir_get_switch(c->tir, node);
    check_value(c, t.condition, RVALUE);

    // First, check all patterns
    for (int32_t i = 0; i < t.branches.len; i += 2) {
        TirId pattern = t.branches.ptr[i];

        if (pattern.id) {
            check_value(c, pattern, RVALUE);
        }
    }

    LinearChecker first_pattern_ctx = *c;
    first_pattern_ctx.var_states = copy_state(&first_pattern_ctx);

    // Then, check each branch for discrepancies
    for (int32_t j = 0; j < t.branches.len; j += 2) {
        TirId value = t.branches.ptr[j + 1];

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
    TirLoop t = tir_get_loop(c->tir, node);

    if (t.init.id) {
        check_value(c, t.init, STATEMENT);
    }

    check_value(c, t.condition, RVALUE);
    int32_t prev_loop_top = c->var_states_loop_top;
    c->var_states_loop_top = c->var_states_top;

    for (int32_t i = 0; i < t.block.len; i++) {
        check_node(c, t.block.ptr[i], STATEMENT);
    }

    if (t.next.id) {
        check_value(c, t.next, STATEMENT);
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
        TirId statement = {get_term_extra(c->tir.thread, block + i)};
        check_node(c, statement, STATEMENT);
    }
}

static void check_node(LinearChecker *c, TirId node, ExpectedValue expected_category) {
    switch (get_tir_tag(c->tir, node)) {
        default: {
            abort();
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR:
        case TIR_INT:
        case TIR_FLOAT:
        case TIR_NULL:
        case TIR_STRING: {
            break;
        }
        case TIR_PARAMETER:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE: {
            int32_t var = tir_get_variable(c->tir, node).index;
            AstId ast_id = c->var_refs[var];
            switch (c->var_states[var]) {
                case VAR_CONSUMED: {
                    error(c, ast_id, Diagnostic(ErrorConsumedValueUsed, {0}));
                    return;
                }
                case VAR_NOT_CONSUMED: {
                    switch (expected_category) {
                        case RVALUE: {
                            c->var_states[var] = VAR_CONSUMED;
                            if (var < c->var_states_loop_top) {
                                error(c, ast_id, Diagnostic(ErrorConsumedInLoop, {0}));
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
                        case RVALUE: error(c, ast_id, Diagnostic(ErrorMoveBorrowed, {0})); break;
                        case LVALUE: /* Allow multiple constant borrows. */ break;
                        case LVALUE_MUT: error(c, ast_id, Diagnostic(ErrorBorrowedMutableShared, {0})); break;
                        case STATEMENT: abort();
                    }
                    return;
                }
                case VAR_BORROWED_MUT: {
                    switch (expected_category) {
                        case RVALUE: error(c, ast_id, Diagnostic(ErrorMoveBorrowed, {0})); break;
                        case LVALUE: error(c, ast_id, Diagnostic(ErrorBorrowedMutableShared, {0})); break;
                        case LVALUE_MUT: error(c, ast_id, Diagnostic(ErrorMultipleMutableBorrows, {0})); break;
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
        case TIR_NOT: {
            check_unary_arit(c, node);
            break;
        }
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
            check_cast(c, node);
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
        case TIR_NEW_STRUCT: {
            check_new_struct(c, node);
            break;
        }
        case TIR_NEW_ARRAY: {
            check_new_array(c, node);
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
        c.file = nth(input->ast_refs, input->functions[i]).ref.file;
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
