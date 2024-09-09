#include "type-analysis.h"

#include "adt.h"
#include "arena.h"
#include "data/ast.h"
#include "data/rir.h"
#include "data/tir.h"
#include "diagnostic.h"
#include "fwd.h"
#include "hash.h"
#include "lex.h"
#include "role-analysis.h"
#include "util.h"
#include "wrappers.h"

#include <math.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    AstId ast_id;
    union {
        int32_t field_index;
        ValueId enum_value;
    };
} TypeScopeSymbol;

typedef struct {
    Vec(TypeScopeSymbol) type_scope_symbols;
    Vec(HashTable) type_scopes;
    Declarations declarations;
} GlobalData;

typedef struct {
    TirRef *tir_refs;
    bool *notes_shown;  // Weather a note to fix an error has been shown already.
} LocalData;

typedef struct {
    Options *options;
    char **paths;
    String *sources;
    Ast *asts;
    Ast *ast;
    Rir *rir;

    Arena *permanent;
    Arena *scratch;

    AstRef *ast_refs;
    TirRef *tir_refs;

    int error;

    int32_t file;
    Locals *local_ast_refs;

    GlobalData *global;
    LocalData *local;
    TirContext tir;
    TypeId current_function_type;
    int32_t loop_depth;
} TypeContext;

static int32_t push_extra(TypeContext *c, int32_t *values, int32_t count) {
    if (!c->local) {
        return 0;
    }

    int32_t index = c->tir.thread->insts.extra.len;
    int32_t *result = vec_grow(&c->tir.thread->insts.extra, count);
    for (int32_t i = 0; i < count; i++) {
        result[i] = values[i];
    }
    return index;
}

static TirId new_inst_impl(TypeContext *c, TirTag tag, AstId ast_id, int32_t left, int32_t right) {
    if (!c->local) {
        return null_tir;
    }

    TirInstData data = {ast_id, left, right};
    sum_vec_push(&c->tir.thread->insts.insts, data, tag);
    return (TirId) {c->tir.thread->insts.insts.len - 1};
}

static ValueId new_unary_inst(TypeContext *c, TirTag tag, AstId ast_id, TypeId type, int32_t operand) {
    TirId tir_id = new_inst_impl(c, tag, ast_id, operand, 0);
    return new_temporary(c->tir, type, tir_id);
}

static ValueId new_binary_inst(TypeContext *c, TirTag tag, AstId ast_id, TypeId type, int32_t left, int32_t right) {
    TirId tir_id = new_inst_impl(c, tag, ast_id, left, right);
    return new_temporary(c->tir, type, tir_id);
}

static TirId new_binary_statement(TypeContext *c, TirTag tag, AstId ast_id, int32_t left, int32_t right) {
    return new_inst_impl(c, tag, ast_id, left, right);
}



static String ctx_source(TypeContext const *c) {
    return c->sources[c->file];
}

static SourceLoc ctx_init_loc(TypeContext const *c, SourceIndex start, size_t len) {
    SourceLoc loc = {
        .path = c->paths[c->file],
        .source = c->sources[c->file],
        .where = start,
        .len = len,
        .mark = start,
    };
    return loc;
}

static void add_error(TypeContext *c) {
    c->error = 1;
}

static SourceIndex get_rir_token(TypeContext *c, AstId ast_id) {
    return get_ast_token(ast_id, &c->asts[c->file]);
}

static void error(TypeContext *c, AstId child, Diagnostic const *diagnostic) {
    SourceIndex child_token = get_rir_token(c, child);
    SourceLoc loc = {
        .path = c->paths[c->file],
        .source = c->sources[c->file],
        .where = child_token,
        .len = 1,
        .mark = child_token,
    };
    print_diagnostic(&loc, diagnostic);
    c->error = 1;
}

static void type_error(TypeContext *c, AstId child, TypeId type, int32_t extra, ErrorKind kind) {
    if (!type.id) {
        return;
    }

    SourceIndex child_token = get_rir_token(c, child);
    SourceLoc loc = {
        .path = c->paths[c->file],
        .source = c->sources[c->file],
        .where = child_token,
        .len = 1,
        .mark = child_token,
    };
    print_diagnostic(&loc, &(Diagnostic) {.kind = kind, .type_error = {c->tir, type, extra}});
    c->error = 1;
}

static void double_type_error(TypeContext *c, AstId child, TypeId type1, TypeId type2, ErrorKind kind) {
    if (!type1.id) {
        return;
    }

    if (!type2.id) {
        return;
    }

    SourceIndex child_token = get_rir_token(c, child);
    SourceLoc loc = {
        .path = c->paths[c->file],
        .source = c->sources[c->file],
        .where = child_token,
        .len = 1,
        .mark = child_token,
    };
    print_diagnostic(&loc, &(Diagnostic) {.kind = kind, .double_type_error = {c->tir, type1, type2}});
    c->error = 1;
}

static int32_t ctx_push_str(TypeContext const *c, String s) {
    char null = '\0';

    if (c->tir.thread) {
        int32_t index = push_str(&c->tir.thread->deps.strtab, s);
        push_str(&c->tir.thread->deps.strtab, (String) {1, &null});
        return index;
    }

    int32_t index = push_str(&c->tir.global->strtab, s);
    push_str(&c->tir.global->strtab, (String) {1, &null});
    return index;
}

static int32_t ctx_push_double_str(TypeContext const *c, String s1, String s2) {
    char null = '\0';

    if (c->tir.thread) {
        int32_t index = push_str(&c->tir.thread->deps.strtab, s1);
        push_str(&c->tir.thread->deps.strtab, s2);
        push_str(&c->tir.thread->deps.strtab, (String) {1, &null});
        return index;
    }

    int32_t index = push_str(&c->tir.global->strtab, s1);
    push_str(&c->tir.global->strtab, s2);
    push_str(&c->tir.global->strtab, (String) {1, &null});
    return index;
}

// Node analysis

static ValueId analyze_value(TypeContext *c, AstId node, TypeId hint);
static TypeId analyze_type(TypeContext *c, AstId node);
static TirId analyze_statement(TypeContext *c, AstId node);

static ValueId expect_mutable_place(TypeContext *c, AstId node, TypeId hint) {
    ValueId result = analyze_value(c, node, hint);
    switch (get_value_category(c->tir, result)) {
        case VALUE_INVALID: return null_value;
        case VALUE_MUTABLE_PLACE: return result;
        default: break;
    }

    error(c, node, &(Diagnostic) {.kind = ERROR_EXPECTED_MUTABLE_PLACE});

    if (get_rir_tag(node, c->rir) == RIR_LOCAL_ID) {
        LocalId local = {get_rir_data(node, c->rir)};
        LocalAstRef ref = c->local_ast_refs[c->file].ptr[local.id];
        Ast *nodes = &c->asts[c->file];

        if (!c->local->notes_shown[local.id] && get_ast_tag(ref.node, nodes) == AST_LET) {
            c->local->notes_shown[local.id] = true;
            SourceIndex token = get_ast_token(ref.node, nodes);
            String name = id_token_to_string(ctx_source(c), token);
            SourceLoc note = ctx_init_loc(c, token, name.len);
            print_diagnostic(&note, &(Diagnostic) {.kind = NOTE_REPLACE_LET_WITH_MUT});
        }
    }

    return null_value;
}

static ValueId apply_implicit_conversion(TypeContext *c, AstId node, ValueId value, TypeId wanted_type) {
    if (!wanted_type.id) {
        return value;
    }

    TypeId provided = get_value_type(c->tir, value);
    if (provided.id == wanted_type.id) {
        return value;
    }

    // Types don't match. Attempt implicit conversion.
    TypeId types[] = {provided, wanted_type};
    TypeId t[2] = {0};

    // *mut T[n] to @mut T
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_MUT_POINTER,
                .inner = &match_array(match_T(2), match_T(1), match_ignore),
            },
            {
                .match_type = TYPE_MATCH_MUT_SLICE,
                .inner = &match_T(1),
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_binary_inst(
                c,
                TIR_ARRAY_TO_SLICE,
                node,
                wanted_type,
                value.id,
                t[1].id
            );
        }
    }
    // *(mut) T[n] to @T
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_ANY_POINTER,
                .inner = &match_array(match_T(2), match_T(1), match_ignore),
            },
            {
                .match_type = TYPE_MATCH_SLICE,
                .inner = &match_T(1),
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_binary_inst(
                c,
                TIR_ARRAY_TO_SLICE,
                node,
                wanted_type,
                value.id,
                t[1].id
            );
        }
    }
    // *mut T to *T
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_MUT_POINTER,
                .inner = &match_T(1),
            },
            {
                .match_type = TYPE_MATCH_POINTER,
                .inner = &match_T(1),
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_unary_inst(c, TIR_NOP, node, wanted_type, value.id);
        }
    }
    // @mut T to @T
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_MUT_SLICE,
                .inner = &match_T(1),
            },
            {
                .match_type = TYPE_MATCH_SLICE,
                .inner = &match_T(1),
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_unary_inst(c, TIR_NOP, node, wanted_type, value.id);
        }
    }
    // *mut T to *mut byte
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_MUT_POINTER,
                .inner = &match_T(1),
            },
            {
                .match_type = TYPE_MATCH_MUT_POINTER,
                .inner = &(TypeMatcher) {.match_type = TYPE_MATCH_BYTE},
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_unary_inst(c, TIR_PTR_CAST, node, wanted_type, value.id);
        }
    }
    // *(mut) T to *byte
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_ANY_POINTER,
                .inner = &match_T(1),
            },
            {
                .match_type = TYPE_MATCH_POINTER,
                .inner = &(TypeMatcher) {.match_type = TYPE_MATCH_BYTE},
            },
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_unary_inst(c, TIR_PTR_CAST, node, wanted_type, value.id);
        }
    }
    // tag[Tags...] to tag:inner
    {
        TypeMatcher matchers[] = {
            {
                .match_type = TYPE_MATCH_TAGGED,
                .inner = &match_T(1),
            },
            match_T(1),
        };
        if (match_types(c->tir, t, 2, types, matchers) && t[0].id) {
            return new_unary_inst(c, TIR_NOP, node, wanted_type, value.id);
        }
    }

    double_type_error(c, node, wanted_type, provided, ERROR_EXPECTED_VALUE_TYPE);
    return null_value;
}

static ValueId expect_value_type(TypeContext *c, AstId node, TypeId wanted_type) {
    ValueId result = analyze_value(c, node, wanted_type);
    return apply_implicit_conversion(c, node, result, wanted_type);
}

typedef struct {
    enum {
        CONSTANT_INVALID,
        CONSTANT_INT,
        CONSTANT_FLOAT,
    } type;

    union {
        int64_t i;
        double f;
    };
} Constant;

static Constant try_get_const(TypeContext *c, ValueId value) {
    switch (get_value_tag(c->tir, value)) {
        case VAL_CONST_INT: return (Constant) {.type = CONSTANT_INT, .i = get_value_int(c->tir, value)};
        case VAL_CONST_FLOAT: return (Constant) {.type = CONSTANT_FLOAT, .f = get_value_float(c->tir, value)};
        default: return (Constant) {.type = CONSTANT_INVALID};
    }
}

static bool try_get_int_const(TypeContext *c, ValueId value, int64_t *i) {
    if (get_value_tag(c->tir, value) == VAL_CONST_INT) {
        *i = get_value_int(c->tir, value);
        return true;
    }

    return false;
}

static TypeId analyze_return_type(TypeContext *c, AstId node) {
    TypeId ret = type_void;
    if (!is_ast_null(node)) {
        ret = analyze_type(c, node);
        if (type_is_unknown_size(c->tir, ret)) {
            type_error(c, node, ret, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
        }
    }
    return ret;
}

// Global nodes

static TirRef analyze_function_decl(TypeContext *c, AstId node) {
    AstFunction f = get_ast_function(node, c->ast);

    for (int32_t i = 0; i < f.type_param_count; i++) {
        int32_t sym = get_rir_data(f.type_params[i], c->rir);
        SourceIndex token = get_rir_token(c, f.type_params[i]);
        String name = id_token_to_string(ctx_source(c), token);
        c->local->tir_refs[sym] = (TirRef) {.type = new_type_parameter(c->tir, i, ctx_push_str(c, name))};
    }

    TypeId *param_types = arena_alloc(c->scratch, TypeId, f.param_count);
    for (int32_t i = 0; i < f.param_count; i++) {
        AstId param_type = get_ast_unary(f.params[i], c->ast);
        param_types[i] = analyze_type(c, param_type);
    }

    TypeId ret_type = analyze_return_type(c, f.ret);
    TypeId type = new_function_type(c->tir, f.type_param_count, f.param_count, param_types, ret_type);

    SourceIndex token = get_rir_token(c, node);
    String name = id_token_to_string(ctx_source(c), token);
    char name_buffer[64];
    int length = snprintf(name_buffer, sizeof(name_buffer), "file%d_", c->file);
    ValueId value = new_function(c->tir, type, ctx_push_double_str(c, (String) {length, name_buffer}, name));

    if (equals(name, (String) Str("main"))) {
        if (f.param_count || !is_ast_null(f.ret)) {
            error(c, node, &(Diagnostic) {.kind = ERROR_MAIN_SIGNATURE});
        }
        c->global->declarations.main = value;
    }

    vec_push(&c->global->declarations.functions, value);
    return (TirRef) {.value = value};
}

typedef struct {
    int32_t index;
    int32_t length;
} TirBlock;

static TirBlock analyze_block(TypeContext *c, AstId block) {
    AstList list = get_ast_list(block, c->ast);
    int32_t *body_tir = arena_alloc(c->scratch, int32_t, list.count);
    int32_t length = 0;
    for (int32_t i = 0; i < list.count; i++) {
        TirId statement_tir = analyze_statement(c, list.nodes[i]);
        if (statement_tir.id) {
            body_tir[length++] = statement_tir.id;
        }
    }
    return (TirBlock) {push_extra(c, body_tir, length), length};
}

static TirId analyze_function(TypeContext *c, AstId node, ValueId value) {
    AstFunction f = get_ast_function(node, c->ast);
    TypeId type = get_value_type(c->tir, value);
    FunctionType func_type = get_function_type(c->tir, type);
    for (int32_t i = 0; i < func_type.param_count; i++) {
        int32_t sym = get_rir_data(f.params[i], c->rir);
        TypeId param_type = get_function_type_param(c->tir, type, i);
        ValueId param_value = new_variable(c->tir, param_type, false);
        c->local->tir_refs[sym] = (TirRef) {.value = param_value};
    }
    c->current_function_type = type;
    TirBlock tir_block = analyze_block(c, f.body);
    if (tir_block.length == 0 && func_type.ret.id != TYPE_VOID) {
        error(c, f.body, &(Diagnostic) {.kind = ERROR_MISSING_RETURN});
    }
    return new_binary_statement(c, TIR_FUNCTION, f.body, tir_block.index, tir_block.length);
}

static TirRef analyze_enum(TypeContext *c, AstId node) {
    AstEnum e = get_ast_enum(node, c->ast);

    TypeId repr_type = analyze_type(c, e.repr);
    if (!type_is_int(repr_type)) {
        type_error(c, e.repr, repr_type, 0, ERROR_ENUM_EXPECTS_INT_TYPE);
        repr_type = null_type;
    }

    SourceIndex token = get_rir_token(c, node);
    String name = id_token_to_string(ctx_source(c), token);
    HashTable table_init = htable_init();
    int32_t scope = c->global->type_scopes.len;
    vec_push(&c->global->type_scopes, table_init);
    TypeId type = new_enum_type(c->tir, scope, ctx_push_str(c, name), repr_type);
    HashTable *table = &c->global->type_scopes.ptr[scope];

    for (int32_t i = 0; i < e.member_count; i++) {
        SourceIndex member_token = get_rir_token(c, e.members[i]);
        String member_name = id_token_to_string(ctx_source(c), member_token);
        int32_t member_sym = c->global->type_scope_symbols.len;
        int64_t prev = htable_try_insert(table, member_name, member_sym);

        if (prev >= 0) {
            SourceLoc loc = ctx_init_loc(c, member_token, member_name.len);
            print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_DEFINITION});
            AstId prev_ref = c->global->type_scope_symbols.ptr[prev].ast_id;
            SourceIndex prev_token = get_ast_token(prev_ref, &c->asts[c->file]);
            SourceLoc prev_loc = ctx_init_loc(c, prev_token, member_name.len);
            print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
            add_error(c);
        }

        ValueId value = new_int_constant(c->tir, type, i);
        vec_push(&c->global->type_scope_symbols, (TypeScopeSymbol) {.ast_id = e.members[i], .field_index = value.id});
    }

    return (TirRef) {.type = type};
}

static TirRef analyze_struct(TypeContext *c, AstId node) {
    AstStruct s = get_ast_struct(node, c->ast);

    for (int32_t i = 0; i < s.type_param_count; i++) {
        int32_t sym = get_rir_data(s.type_params[i], c->rir);
        SourceIndex token = get_rir_token(c, s.type_params[i]);
        String name = id_token_to_string(ctx_source(c), token);
        c->local->tir_refs[sym] = (TirRef) {.type = new_type_parameter(c->tir, i, ctx_push_str(c, name))};
    }

    TypeId *field_types = arena_alloc(c->scratch, TypeId, s.field_count);
    for (int32_t i = 0; i < s.field_count; i++) {
        AstId param_type = get_ast_unary(s.fields[i], c->ast);
        field_types[i] = analyze_type(c, param_type);
    }

    HashTable table = htable_init();
    int32_t index = 0;

    for (int32_t i = 0; i < s.field_count; i++) {
        SourceIndex field_token = get_rir_token(c, s.fields[i]);
        String field_name = id_token_to_string(ctx_source(c), field_token);

        int32_t field_sym = c->global->type_scope_symbols.len;
        vec_push(&c->global->type_scope_symbols, (TypeScopeSymbol) {.ast_id = s.fields[i], .field_index = index});
        int64_t prev = htable_try_insert(&table, field_name, field_sym);

        if (prev >= 0) {
            SourceLoc loc = ctx_init_loc(c, field_token, field_name.len);
            print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_DEFINITION});
            AstId prev_ref = c->global->type_scope_symbols.ptr[prev].ast_id;
            SourceIndex prev_token = get_ast_token(prev_ref, &c->asts[c->file]);
            SourceLoc prev_loc = ctx_init_loc(c, prev_token, field_name.len);
            print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
            add_error(c);
        }

        index++;
    }

    int32_t scope = c->global->type_scopes.len;
    vec_push(&c->global->type_scopes, table);

    SourceIndex token = get_rir_token(c, node);
    String name = id_token_to_string(ctx_source(c), token);

    if (!s.field_count) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EMPTY_STRUCT});
    }

    TypeId type = new_struct_type(c->tir, scope, ctx_push_str(c, name), s.type_param_count, s.field_count, field_types, c->options->target);
    vec_push(&c->global->declarations.structs, type);
    return (TirRef) {.type = type};
}

static TirRef analyze_newtype(TypeContext *c, AstId node) {
    AstNewtype n = get_ast_newtype(node, c->ast);
    TypeId alias = analyze_type(c, n.type);
    SourceIndex token = get_rir_token(c, node);
    String name = id_token_to_string(ctx_source(c), token);
    return (TirRef) {.type = new_newtype_type(c->tir, ctx_push_str(c, name), n.count, alias)};
}

static TirRef analyze_extern_function(TypeContext *c, AstId node) {
    AstFunction f = get_ast_extern_function(node, c->ast);
    TypeId *param_types = arena_alloc(c->scratch, TypeId, f.param_count);
    for (int32_t i = 0; i < f.param_count; i++) {
        AstId param_type = get_ast_unary(f.params[i], c->ast);
        param_types[i] = analyze_type(c, param_type);
    }

    TypeId ret_type = type_void;
    if (!is_ast_null(f.ret)) {
        ret_type = analyze_type(c, f.ret);
    }

    TypeId type = new_function_type(c->tir, 0, f.param_count, param_types, ret_type);
    SourceIndex token = get_rir_token(c, node);
    String name = id_token_to_string(ctx_source(c), token);

    ValueId value = new_extern_function(c->tir, type, ctx_push_str(c, name));
    vec_push(&c->global->declarations.extern_functions, value);
    return (TirRef) {.value = value};
}

static TirRef analyze_extern_mut(TypeContext *c, AstId node) {
    AstId var_type = get_ast_unary(node, c->ast);
    TypeId type = analyze_type(c, var_type);
    String name = id_token_to_string(ctx_source(c), get_rir_token(c, node));
    ValueId value = new_extern_var(c->tir, type, ctx_push_str(c, name));
    vec_push(&c->global->declarations.extern_vars, value);
    return (TirRef) {.value = value};
}

static TirRef analyze_type_alias(TypeContext *c, AstId node) {
    AstId init = get_ast_unary(node, c->ast);
    TypeId type = analyze_type(c, init);
    return (TirRef) {.type = type};
}

static TirRef analyze_const(TypeContext *c, AstId node) {
    AstId init = get_ast_unary(node, c->ast);
    ValueId init_result = analyze_value(c, init, null_type);

    switch (get_value_tag(c->tir, init_result)) {
        case VAL_ERROR: {
            return (TirRef) {0};
        }
        case VAL_CONST_INT:
        case VAL_CONST_FLOAT:
        case VAL_CONST_NULL: {
            return (TirRef) {.value = init_result};
        }
        default: {
            error(c, init, &(Diagnostic) {.kind = ERROR_CONST_INIT});
            return (TirRef) {0};
        }
    }
}

// Local nodes

static TirId analyze_let(TypeContext *c, AstId node, bool mutable) {
    AstId init = get_ast_unary(node, c->ast);
    ValueId init_value = analyze_value(c, init, null_type);
    TypeId init_type = get_value_type(c->tir, init_value);
    if (type_is_unknown_size(c->tir, init_type)) {
        type_error(c, node, init_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    }
    int32_t var = c->tir.thread->local_count;
    ValueId value = new_variable(c->tir, init_type, mutable);
    TirId inst = new_inst_impl(c, mutable ? TIR_MUT : TIR_LET, node, var, init_value.id);
    int32_t sym = get_rir_data(node, c->rir);
    c->local->tir_refs[sym] = (TirRef) {.value = value};
    return inst;
}

static TypeId analyze_function_type(TypeContext *c, AstId node) {
    AstCall signature = get_ast_call(node, c->ast);

    TypeId *params = arena_alloc(c->scratch, TypeId, signature.arg_count);
    for (int32_t i = 0; i < signature.arg_count; i++) {
        AstId param_type = get_ast_unary(signature.args[i], c->ast);
        params[i] = analyze_type(c, param_type);
    }

    TypeId ret = analyze_return_type(c, signature.operand);
    return new_function_type(c->tir, 0, signature.arg_count, params, ret);
}

static TypeId analyze_array_type(TypeContext *c, AstId node) {
    AstBinary array = get_ast_binary(node, c->ast);
    TypeId index = analyze_type(c, array.left);
    TypeId element = analyze_type(c, array.right);

    if (get_type_tag(c->tir, index) != TYPE_ARRAY_LENGTH) {
        type_error(c, array.left, index, 0, ERROR_ARRAY_TYPE_EXPECTS_LENGTH_TYPE);
        index = null_type;
    }

    return new_array_type(c->tir, index, element);
}

static TypeId analyze_array_type_sugar(TypeContext *c, AstId node) {
    AstBinary array = get_ast_binary(node, c->ast);
    ValueId length_result = expect_value_type(c, array.left, type_isize);
    int64_t len = 0;
    TypeId index = try_get_int_const(c, length_result, &len) ? new_array_length_type(c->tir, len) : null_type;
    TypeId element = analyze_type(c, array.right);
    return new_array_type(c->tir, index, element);
}

static TypeId analyze_type_id(TypeContext *c, AstId node, SymbolKind kind) {
    int32_t id = get_rir_data(node, c->rir);
    switch (kind) {
        case SYM_UNDEFINED: {
            break;
        }
        case SYM_BUILTIN: {
            switch ((BuiltinId) id) {
                case BUILTIN_SIZE_TAG: return type_size_tag;
                case BUILTIN_ALIGNMENT_TAG: return type_alignment_tag;
                #define TYPE(type) case BUILTIN_##type: return type_##type;
                #include "simple-types"
                default: break;
            }
            break;
        }
        case SYM_GLOBAL: {
            return c->tir_refs[id].type;
        }
        case SYM_LOCAL: {
            return c->local->tir_refs[id].type;
        }
    }
    return null_type;
}

static ValueId analyze_value_id(TypeContext *c, AstId node, SymbolKind kind) {
    int32_t id = get_rir_data(node, c->rir);
    TirRef info;
    switch (kind) {
        case SYM_GLOBAL: info = c->tir_refs[id]; break;
        case SYM_LOCAL: info = c->local->tir_refs[id]; break;
        default: return null_value;
    }
    return info.value;
}

static ValueId analyze_int(TypeContext *c, AstId node, TypeId hint) {
    int64_t i = get_ast_int(node, c->ast);
    TypeId type = int_fits_in_type(i, hint, c->options->target) ? hint : type_i64;
    return new_int_constant(c->tir, type, i);
}

static ValueId analyze_float(TypeContext *c, AstId node, TypeId hint) {
    double f = get_ast_float(node, c->ast);
    TypeId type = type_is_float(hint) ? hint : type_f64;
    return new_float_constant(c->tir, type, f);
}

static ValueId analyze_char(TypeContext *c, AstId node) {
    int64_t i = get_ast_int(node, c->ast);
    return new_int_constant(c->tir, type_char, i);
}

static int parse_hex_char(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static ValueId analyze_string(TypeContext *c, AstId node) {
    int64_t len = string_token_byte_length(ctx_source(c), get_rir_token(c, node));
    int32_t index;
    char *buffer;

    if (c->tir.thread) {
        index = c->tir.thread->deps.strtab.len;
        buffer = vec_grow(&c->tir.thread->deps.strtab, len + 4);
    } else {
        index = c->tir.global->strtab.len;
        buffer = vec_grow(&c->tir.global->strtab, len + 4);
    }

    buffer[0] = (unsigned char) (len & 0xFF);
    buffer[1] = (unsigned char) ((len >> 8) & 0xFF);
    buffer[2] = (unsigned char) ((len >> 16) & 0xFF);
    buffer[3] = (unsigned char) ((len >> 24) & 0xFF);
    int32_t token = get_rir_token(c, node).index;

    char const *str = ctx_source(c).ptr + token;
    ptrdiff_t i = 1;
    ptrdiff_t byte_i = 4;
    while (str[i] != '\"' && str[i] != '\n' && str[i] != '\0') {
        if (str[i] == '\\') {
            i++;
            switch (str[i++]) {
                case 't': buffer[byte_i++] = '\t'; break;
                case 'n': buffer[byte_i++] = '\n'; break;
                case '"': buffer[byte_i++] = '"'; break;
                case '\'': buffer[byte_i++] = '\''; break;
                case '\\': buffer[byte_i++] = '\\'; break;
                case 'x': {
                    buffer[byte_i++] = (unsigned char) ((parse_hex_char(str[i]) << 4) | parse_hex_char(str[i + 1]));
                    i += 2;
                    break;
                }
                default: {
                    SourceLoc loc = ctx_init_loc(c, (SourceIndex) {token + i - 2}, 2);
                    loc.mark = loc.where;
                    print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_ESCAPE_SEQUENCE});
                    c->error = 1;
                    break;
                }
            }
            continue;
        }
        buffer[byte_i++] = str[i];
        i++;
    }

    if (str[i] != '\"') {
        SourceLoc loc = ctx_init_loc(c, (SourceIndex) {token + i}, 1);
        loc.mark = loc.where;
        print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_UNTERMINATED_STRING});
        c->error = 1;
    }

    TypeId type = new_array_type(c->tir, new_array_length_type(c->tir, len), type_char);
    return new_string_constant(c->tir, type, index);
}

static ValueId analyze_bool(TypeContext *c, AstId node) {
    int64_t i = get_ast_int(node, c->ast);
    return new_int_constant(c->tir, type_bool, i);
}

static ValueId analyze_null(TypeContext *c, TypeId hint) {
    TypeId type = hint;
    if (!remove_pointer(c->tir, hint).id) {
        type = new_ptr_type(c->tir, TYPE_PTR_MUT, type_byte);
    }
    return new_null_constant(c->tir, type);
}

static ValueId analyze_un_arithmetic(TypeContext *c, AstId node, TypeId hint, TirTag tag) {
    AstId operand = get_ast_unary(node, c->ast);
    ValueId operand_value = analyze_value(c, operand, hint);
    TypeId operand_type = get_value_type(c->tir, operand_value);

    if (!type_is_arithmetic(operand_type)) {
        type_error(c, operand, operand_type, 0, ERROR_UNARY_UNEXPECTED_OPERAND);
        return null_value;
    }

    Constant operand_c = try_get_const(c, operand_value);
    switch (operand_c.type) {
        case CONSTANT_INVALID: {
            break;
        }
        case CONSTANT_INT: {
            int64_t r;
            bool overflow;
            switch (tag) {
                case TIR_PLUS: return operand_value;
                case TIR_MINUS: overflow = operand_c.i == INT64_MIN ? true : (r = -operand_c.i, false); break;
                default: abort();
            }
            if (overflow) {
                error(c, node, &(Diagnostic) {.kind = ERROR_CONST_INT_OVERFLOW});
                return null_value;
            }
            return new_int_constant(c->tir, operand_type, r);
        }
        case CONSTANT_FLOAT: {
            double r;
            switch (tag) {
                case TIR_PLUS: return operand_value;
                case TIR_MINUS: r = -operand_c.f; break;
                default: abort();
            }
            return new_float_constant(c->tir, operand_type, r);
        }
    }

    return new_unary_inst(c, tag, node, operand_type, operand_value.id);
}

static ValueId analyze_not(TypeContext *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    ValueId operand_value = expect_value_type(c, operand, type_bool);
    Constant operand_c = try_get_const(c, operand_value);
    if (operand_c.type == CONSTANT_INT) {
        return new_int_constant(c->tir, type_bool, !operand_c.i);
    }
    return new_unary_inst(c, TIR_NOT, node, type_bool, operand_value.id);
}

static ValueId analyze_address(TypeContext *c, AstId node, TypeId hint) {
    AstId operand = get_ast_unary(node, c->ast);
    ValueId operand_value = analyze_value(c, operand, remove_any_pointer(c->tir, hint));
    TypeId operand_type = get_value_type(c->tir, operand_value);
    switch (get_value_category(c->tir, operand_value)) {
        case VALUE_INVALID: {
            break;
        }
        case VALUE_TEMPORARY: {
            TypeId type = new_ptr_type(c->tir, TYPE_PTR_MUT, operand_type);
            return new_unary_inst(c, TIR_ADDRESS_OF_TEMPORARY, node, type, operand_value.id);
        }
        case VALUE_PLACE: {
            TypeId type = new_ptr_type(c->tir, TYPE_PTR, operand_type);
            return new_unary_inst(c, TIR_ADDRESS, node, type, operand_value.id);
        }
        case VALUE_MUTABLE_PLACE: {
            TypeId type = new_ptr_type(c->tir, TYPE_PTR_MUT, operand_type);
            return new_unary_inst(c, TIR_ADDRESS, node, type, operand_value.id);
        }
    }
    return null_value;
}

static ValueId analyze_multiaddress(TypeContext *c, AstId node, TypeId hint) {
    AstId operand = get_ast_unary(node, c->ast);
    return analyze_value(c, operand, hint);
}

static ValueId analyze_deref(TypeContext *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    ValueId operand_value = analyze_value(c, operand, null_type);
    TypeId operand_type = get_value_type(c->tir, operand_value);

    TypeId type = remove_pointer(c->tir, operand_type);
    if (!type.id) {
        type_error(c, operand, operand_type, 0, ERROR_DEREF_UNEXPECTED_OPERAND);
        return null_value;
    }

    return new_unary_inst(c, TIR_DEREF, node, type, operand_value.id);
}

static TypeId analyze_ptr(TypeContext *c, AstId node, TypeTag tag) {
    AstId operand = get_ast_unary(node, c->ast);
    TypeId operand_type = analyze_type(c, operand);
    return new_ptr_type(c->tir, tag, operand_type);
}

static TypeId analyze_multiptr(TypeContext *c, AstId node, TypeTag tag) {
    AstId operand = get_ast_unary(node, c->ast);
    TypeId operand_type = analyze_type(c, operand);
    return new_multiptr_type(c->tir, tag, operand_type);
}

static AstId get_call_arg(AstCall *call, int32_t i) {
    return i < call->arg_count ? call->args[i] : null_ast;
}

static bool expect_arg_count(TypeContext *c, AstId node, int32_t param_count) {
    AstCall call = get_ast_call(node, c->ast);
    if (call.arg_count == param_count) {
        return true;
    }
    error(c, call.operand, &(Diagnostic) {.kind = ERROR_WRONG_COUNT, .count_error = {param_count, call.arg_count}});
    return false;
}

static ValueId analyze_alignof(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TypeId operand_type = analyze_type(c, get_call_arg(&call, 0));
    int32_t i = alignof_type(c->tir, operand_type, c->options->target);
    expect_arg_count(c, node, 1);
    if (i >= 1) {
        TypeId type = new_tagged_type(c->tir, type_alignment_tag, type_isize, 1, &operand_type);
        return new_int_constant(c->tir, type, i);
    }
    type_error(c, node, operand_type, 0, ERROR_TYPE_UNKNOWN_TYPE_ALIGNMENT);
    return null_value;
}

static ValueId analyze_sizeof(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TypeId operand_type = analyze_type(c, get_call_arg(&call, 0));
    int64_t i = sizeof_type(c->tir, operand_type, c->options->target);
    expect_arg_count(c, node, 1);
    if (i >= 1) {
        TypeId type = new_tagged_type(c->tir, type_size_tag, type_isize, 1, &operand_type);
        return new_int_constant(c->tir, type, i);
    }
    type_error(c, node, operand_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    return null_value;
}

static ValueId analyze_zero_extend(TypeContext *c, AstId node, TypeId hint) {
    AstCall call = get_ast_call(node, c->ast);
    ValueId operand_value = analyze_value(c, get_call_arg(&call, 0), hint);
    expect_arg_count(c, node, 1);

    if (!hint.id || !type_is_fixed_int(hint)) {
        error(c, node, &(Diagnostic) {.kind = ERROR_TYPE_INFERENCE});
        return null_value;
    }

    TypeId operand_type = get_value_type(c->tir, operand_value);
    if (!type_is_fixed_int(operand_type)) {
        double_type_error(c, call.operand, operand_type, hint, ERROR_CAST);
        return null_value;
    }

    if (bigger_primitive_type(hint, operand_type, c->options->target).id == operand_type.id) {
        return operand_value;
    }

    Constant operand_c = try_get_const(c, operand_value);
    if (operand_c.type == CONSTANT_INT) {
        int64_t int_size = sizeof_type(c->tir, operand_type, c->options->target);
        union {
            uint64_t u;
            int64_t s;
        } mask;
        mask.u = ((uint64_t) 1 << (int_size * 8)) - 1;
        return new_int_constant(c->tir, hint, operand_c.i & mask.s);
    }

    return new_unary_inst(c, TIR_ZEXT, node, hint, operand_value.id);
}

static ValueId analyze_slice_constructor(TypeContext *c, AstId node, TypeId hint) {
    AstCall call = get_ast_call(node, c->ast);
    ValueId length_result = expect_value_type(c, get_call_arg(&call, 0), type_isize);
    ValueId data_value = analyze_value(c, get_call_arg(&call, 1), replace_slice_with_pointer(c->tir, hint));
    expect_arg_count(c, node, 2);
    TypeId data_type = get_value_type(c->tir, data_value);
    TypeId type = replace_pointer_with_slice(c->tir, data_type);

    if (!type.id && call.arg_count >= 2) {
        type_error(c, get_call_arg(&call, 1), data_type, 0, ERROR_SLICE_CTOR_EXPECTS_POINTER);
        return null_value;
    }

    int32_t extra[2] = {
        length_result.id,
        data_value.id,
    };
    return new_binary_inst(c, TIR_NEW_STRUCT, node, type, push_extra(c, extra, ArrayLength(extra)), 2);
}

static TypeId analyze_linear(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TypeId operand_type = analyze_type(c, get_call_arg(&call, 0));
    expect_arg_count(c, node, 1);
    return new_linear_type(c->tir, operand_type);
}

static TypeId analyze_array_length_type(TypeContext *c, AstId node) {
    AstBinary call = get_ast_binary(node, c->ast);
    ValueId operand_value = expect_value_type(c, call.right, type_isize);
    expect_arg_count(c, node, 1);

    int64_t i = 0;
    if (try_get_int_const(c, operand_value, &i)) {
        return new_array_length_type(c->tir, 1);
    }

    return null_type;
}

static ValueId analyze_bin_arithmetic(TypeContext *c, AstId node, TypeId hint, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = analyze_value(c, bin.left, hint);
    TypeId left_type = get_value_type(c->tir, left_value);
    left_type = remove_templ(c->tir, left_type);
    ValueId right_value = analyze_value(c, bin.right, left_type);
    TypeId right_type = get_value_type(c->tir, right_value);
    right_type = remove_templ(c->tir, right_type);

    if (left_type.id != right_type.id || !type_is_arithmetic(left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_value;
    }

    Constant left_c = try_get_const(c, left_value);
    Constant right_c = try_get_const(c, right_value);
    if (left_c.type != CONSTANT_INVALID && left_c.type == right_c.type) {
        switch (left_c.type) {
            case CONSTANT_INVALID: {
                break;
            }
            case CONSTANT_INT: {
                int64_t r;
                bool overflow;
                switch (tag) {
                    case TIR_ADD: overflow = __builtin_add_overflow(left_c.i, right_c.i, &r); break;
                    case TIR_SUB: overflow = __builtin_sub_overflow(left_c.i, right_c.i, &r); break;
                    case TIR_MUL: overflow = __builtin_mul_overflow(left_c.i, right_c.i, &r); break;
                    case TIR_DIV: overflow = (right_c.i == 0 || (left_c.i == INT64_MIN && right_c.i == -1)) ? true : (r = left_c.i / right_c.i, false); break;
                    case TIR_MOD: overflow = (right_c.i == 0) ? true : (r = left_c.i % right_c.i, false); break;
                    default: abort();
                }
                if (overflow || !int_fits_in_type(r, left_type, c->options->target)) {
                    error(c, node, &(Diagnostic) {.kind = ERROR_CONST_INT_OVERFLOW});
                    return null_value;
                }
                return new_int_constant(c->tir, left_type, r);
            }
            case CONSTANT_FLOAT: {
                double r;
                switch (tag) {
                    case TIR_ADD: r = left_c.f + right_c.f; break;
                    case TIR_SUB: r = left_c.f - right_c.f; break;
                    case TIR_MUL: r = left_c.f * right_c.f; break;
                    case TIR_DIV: r = left_c.f / right_c.f; break;
                    case TIR_MOD: r = fmod(left_c.f, right_c.f); break;
                    default: abort();
                }
                return new_float_constant(c->tir, left_type, r);
            }
        }
    }

    return new_binary_inst(c, tag, node, left_type, left_value.id, right_value.id);
}

static ValueId analyze_bin_bit(TypeContext *c, AstId node, TypeId hint, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = analyze_value(c, bin.left, hint);
    TypeId left_type = get_value_type(c->tir, left_value);
    left_type = remove_templ(c->tir, left_type);
    ValueId right_value = analyze_value(c, bin.right, left_type);
    TypeId right_type = get_value_type(c->tir, right_value);
    right_type = remove_templ(c->tir, right_type);

    if (left_type.id != right_type.id || !type_is_int(left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_value;
    }

    Constant left_c = try_get_const(c, left_value);
    Constant right_c = try_get_const(c, right_value);
    if (left_c.type == CONSTANT_INT && left_c.type == right_c.type) {
        int64_t int_size = sizeof_type(c->tir, left_type, c->options->target);
        union {
            uint64_t u;
            int64_t s;
        } mask;
        mask.u = ((uint64_t) 1 << (int_size * 8)) - 1;
        int64_t r;
        switch (tag) {
            case TIR_AND: r = left_c.i & right_c.i; break;
            case TIR_OR: r = left_c.i | right_c.i; break;
            case TIR_XOR: r = left_c.i ^ right_c.i; break;
            case TIR_SHL: {
                if (right_c.i < 0) {
                    error(c, node, &(Diagnostic) {.kind = ERROR_CONST_NEGATIVE_SHIFT});
                    return null_value;
                }
                r = right_c.i >= int_size * 8 ? 0 : (left_c.i << right_c.i);
                if ((r & mask.s) == 0) {
                    r = 0;
                }
                break;
            }
            case TIR_SHR: {
                if (right_c.i < 0) {
                    error(c, node, &(Diagnostic) {.kind = ERROR_CONST_NEGATIVE_SHIFT});
                    return null_value;
                }
                if (right_c.i >= int_size * 8) {
                    r = left_c.i < 0 ? -1 : 0;
                } else {
                    r = left_c.i >> right_c.i;
                }
                break;
            }
            default: abort();
        }
        return new_int_constant(c->tir, left_type, r);
    }

    return new_binary_inst(c, tag, node, left_type, left_value.id, right_value.id);
}

static ValueId analyze_eq(TypeContext *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = analyze_value(c, bin.left, null_type);
    TypeId left_type = get_value_type(c->tir, left_value);
    ValueId right_value = analyze_value(c, bin.right, left_type);
    TypeId right_type = get_value_type(c->tir, right_value);

    if (left_type.id != right_type.id || !is_equality_type(c->tir, left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_value;
    }

    Constant left_c = try_get_const(c, left_value);
    Constant right_c = try_get_const(c, right_value);
    if (left_c.type != CONSTANT_INVALID && left_c.type == right_c.type) {
        switch (left_c.type) {
            case CONSTANT_INVALID: {
                break;
            }
            case CONSTANT_INT: {
                bool r;
                switch (tag) {
                    case TIR_EQ: r = left_c.i == right_c.i; break;
                    case TIR_NE: r = left_c.i != right_c.i; break;
                    default: abort();
                }
                return new_int_constant(c->tir, type_bool, r);
            }
            case CONSTANT_FLOAT: {
                bool r;
                switch (tag) {
                    case TIR_EQ: r = left_c.f == right_c.f; break;
                    case TIR_NE: r = left_c.f != right_c.f; break;
                    default: abort();
                }
                return new_int_constant(c->tir, type_bool, r);
            }
        }
    }

    return new_binary_inst(c, tag, node, type_bool, left_value.id, right_value.id);
}

static ValueId analyze_rel(TypeContext *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = analyze_value(c, bin.left, null_type);
    TypeId left_type = get_value_type(c->tir, left_value);
    ValueId right_value = analyze_value(c, bin.right, left_type);
    TypeId right_type = get_value_type(c->tir, right_value);

    if (left_type.id != right_type.id || !is_relative_type(c->tir, left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_value;
    }

    Constant left_c = try_get_const(c, left_value);
    Constant right_c = try_get_const(c, right_value);
    if (left_c.type != CONSTANT_INVALID && left_c.type == right_c.type) {
        switch (left_c.type) {
            case CONSTANT_INVALID: {
                break;
            }
            case CONSTANT_INT: {
                bool r;
                switch (tag) {
                    case TIR_LT: r = left_c.i < right_c.i; break;
                    case TIR_GT: r = left_c.i > right_c.i; break;
                    case TIR_LE: r = left_c.i <= right_c.i; break;
                    case TIR_GE: r = left_c.i >= right_c.i; break;
                    default: abort();
                }
                return new_int_constant(c->tir, type_bool, r);
            }
            case CONSTANT_FLOAT: {
                bool r;
                switch (tag) {
                    case TIR_LT: r = left_c.f < right_c.f; break;
                    case TIR_GT: r = left_c.f > right_c.f; break;
                    case TIR_LE: r = left_c.f <= right_c.f; break;
                    case TIR_GE: r = left_c.f >= right_c.f; break;
                    default: abort();
                }
                return new_int_constant(c->tir, type_bool, r);
            }
        }
    }

    return new_binary_inst(c, tag, node, type_bool, left_value.id, right_value.id);
}

static ValueId analyze_logic(TypeContext *c, AstId node, bool is_and) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = expect_value_type(c, bin.left, type_bool);
    ValueId right_value = expect_value_type(c, bin.right, type_bool);

    Constant left_c = try_get_const(c, left_value);
    Constant right_c = try_get_const(c, right_value);
    if (left_c.type == CONSTANT_INT && left_c.type == right_c.type) {
        bool r;
        if (is_and) {
            r = left_c.i && right_c.i;
        } else {
            r = left_c.i || right_c.i;
        }
        return new_int_constant(c->tir, type_bool, r);
    }

    ValueId true_value = new_int_constant(c->tir, type_bool, 1);
    ValueId false_value = new_int_constant(c->tir, type_bool, 0);

    int32_t branches_tir[4] = {
        is_and ? false_value.id : true_value.id, is_and ? false_value.id : true_value.id,
        0, right_value.id,
    };
    int32_t extra[] = {
        push_extra(c, branches_tir, 4),
        2,
    };
    return new_binary_inst(c, TIR_SWITCH, node, type_bool, left_value.id, push_extra(c, extra, ArrayLength(extra)));
}

static ValueId analyze_assign(TypeContext *c, AstId node) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = expect_mutable_place(c, bin.left, null_type);
    TypeId left_type = get_value_type(c->tir, left_value);
    ValueId right_value = expect_value_type(c, bin.right, left_type);
    if (type_is_unknown_size(c->tir, left_type)) {
        type_error(c, node, left_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    }
    return new_binary_inst(c, TIR_ASSIGN, node, type_void, left_value.id, right_value.id);
}

static ValueId analyze_assign_arithmetic(TypeContext *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = expect_mutable_place(c, bin.left, null_type);
    TypeId left_type = get_value_type(c->tir, left_value);
    ValueId right_value = expect_value_type(c, bin.right, left_type);
    if (!type_is_arithmetic(left_type)) {
        double_type_error(c, bin.left, left_type, get_value_type(c->tir, right_value), ERROR_BINARY_UNEXPECTED_OPERANDS);
    }
    return new_binary_inst(c, tag, node, type_void, left_value.id, right_value.id);
}

static ValueId analyze_assign_bit(TypeContext *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    ValueId left_value = expect_mutable_place(c, bin.left, null_type);
    TypeId left_type = get_value_type(c->tir, left_value);
    ValueId right_value = expect_value_type(c, bin.right, left_type);
    if (!type_is_int(left_type)) {
        double_type_error(c, bin.left, left_type, get_value_type(c->tir, right_value), ERROR_BINARY_UNEXPECTED_OPERANDS);
    }
    return new_binary_inst(c, tag, node, type_void, left_value.id, right_value.id);
}

static ValueId implicit_pointer_deref(TypeContext *c, AstId node, ValueId value) {
    TypeId type = get_value_type(c->tir, value);
    TypeId inner_type = remove_pointer(c->tir, type);
    if (!inner_type.id) {
        return value;
    }
    return new_unary_inst(c, TIR_DEREF, node, inner_type, value.id);
}

static int32_t find_field(TypeContext *c, TypeId type, String name) {
    int32_t scope = get_struct_type(c->tir, type).scope;
    uint32_t *sym = htable_lookup(&c->global->type_scopes.ptr[scope], name);

    if (!sym) {
        return -1;
    }

    return *sym;
}

static ValueId resolve_enum_member(TypeContext *c, AstId node, TypeId type) {
    SourceIndex field_token = get_rir_token(c, node);
    String field_name = id_token_to_string(ctx_source(c), field_token);
    int32_t scope = get_enum_type(c->tir, type).scope;
    uint32_t *sym_ptr = htable_lookup(&c->global->type_scopes.ptr[scope], field_name);

    if (!sym_ptr) {
        type_error(c, node, type, 0, ERROR_UNDEFINED_TYPE_SCOPE);
        return null_value;
    }

    TypeScopeSymbol sym = c->global->type_scope_symbols.ptr[*sym_ptr];
    return (ValueId) {sym.field_index};
}

static ValueId analyze_enum_member(TypeContext *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TypeId type = analyze_type(c, operand);

    if (get_type_tag(c->tir, type) != TYPE_ENUM) {
        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_SCOPE);
        return null_value;
    }

    return resolve_enum_member(c, node, type);
}

static ValueId analyze_enum_member_inferred(TypeContext *c, AstId node, TypeId hint) {
    if (!hint.id || get_type_tag(c->tir, hint) != TYPE_ENUM) {
        error(c, node, &(Diagnostic) {.kind = ERROR_TYPE_INFERENCE});
        return null_value;
    }

    return resolve_enum_member(c, node, hint);
}

static ValueId resolve_length(TypeContext *c, AstId node, ValueId array_like) {
    TypeId type = get_value_type(c->tir, array_like);
    switch (get_type_tag(c->tir, type)) {
        case TYPE_ARRAY: {
            TypeId index_type = get_array_type(c->tir, type).index;
            return new_int_constant(c->tir, type_isize, get_array_length_type(c->tir, index_type));
        }
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: {
            return new_binary_inst(c, TIR_ACCESS, node, type_isize, array_like.id, 0);
        }
        default: {
            return null_value;
        }
    }
}

static ValueId resolve_slice_data(TypeContext *c, AstId node, ValueId array_like) {
    TypeId type = get_value_type(c->tir, array_like);
    type = replace_slice_with_pointer(c->tir, type);
    if (!type.id) {
        return null_value;
    }
    return new_binary_inst(c, TIR_ACCESS, node, type, array_like.id, 1);
}

static ValueId analyze_struct_access(TypeContext *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    SourceIndex field_token = get_rir_token(c, node);
    String field_name = id_token_to_string(ctx_source(c), field_token);
    ValueId operand_value = analyze_value(c, operand, null_type);
    operand_value = implicit_pointer_deref(c, node, operand_value);
    TypeId operand_type = get_value_type(c->tir, operand_value);
    TypeId type = remove_templ(c->tir, operand_type);
    TypeId slice_elem_type = remove_slice(c->tir, type);

    if (slice_elem_type.id) {
        if (equals(field_name, (String) Str("length"))) {
            return resolve_length(c, node, operand_value);
        }

        if (equals(field_name, (String) Str("data"))) {
            return resolve_slice_data(c, node, operand_value);
        }

        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_value;
    }

    TypeId array_elem_type = remove_array_like(c->tir, type);

    if (array_elem_type.id) {
        if (equals(field_name, (String) Str("length"))) {
            return resolve_length(c, node, operand_value);
        }

        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_value;
    }

    if (get_type_tag(c->tir, type) != TYPE_STRUCT) {
        type_error(c, operand, operand_type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_value;
    }

    int32_t field_sym = find_field(c, type, field_name);

    if (field_sym == -1) {
        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_value;
    }

    TypeScopeSymbol sym = c->global->type_scope_symbols.ptr[field_sym];
    TypeId result_type = get_struct_type_field(c->tir, type, sym.field_index);
    return new_binary_inst(c, TIR_ACCESS, node, result_type, operand_value.id, sym.field_index);
}

static TirTag get_cast_type(TypeContext *c, TypeId operand_type, TypeId cast_type) {
    if (remove_pointer(c->tir, operand_type).id && remove_pointer(c->tir, cast_type).id) {
        return TIR_PTR_CAST;
    }

    if (type_is_int(operand_type) && type_is_float(cast_type)) {
        return TIR_ITOF;
    }

    if (type_is_float(operand_type) && type_is_int(cast_type)) {
        return TIR_FTOI;
    }

    if (type_is_int(operand_type) && type_is_int(cast_type)) {
        if (operand_type.id == TYPE_char) {
            return TIR_ZEXT;
        }
        return bigger_primitive_type(cast_type, operand_type, c->options->target).id == operand_type.id ? TIR_ITRUNC : TIR_SEXT;
    }

    if (type_is_float(operand_type) && type_is_float(cast_type)) {
        return bigger_primitive_type(cast_type, operand_type, c->options->target).id == operand_type.id ? TIR_FTRUNC : TIR_FEXT;
    }

    return -1;
}

static int64_t truncate_int_const(int64_t i, int64_t bits) {
    union {
        uint64_t u;
        int64_t s;
    } mask;
    mask.u = ((uint64_t) 1 << bits) - 1;
    if (i < 0) {
        return i | ~mask.s;
    } else {
        return i & mask.s;
    }
}

static ValueId analyze_cast(TypeContext *c, AstId node) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TypeId cast_type = analyze_type(c, bin.right);
    ValueId operand_value = analyze_value(c, bin.left, cast_type);
    TypeId operand_type = get_value_type(c->tir, operand_value);

    if (operand_type.id == cast_type.id) {
        return operand_value;
    }

    TirTag cast_kind = get_cast_type(c, operand_type, cast_type);
    if ((int) cast_kind == -1) {
        double_type_error(c, bin.left, operand_type, cast_type, ERROR_CAST);
        return null_value;
    }

    Constant operand_c = try_get_const(c, operand_value);
    if (operand_c.type != CONSTANT_INVALID) {
        switch (cast_kind) {
            case TIR_ITOF: {
                return new_float_constant(c->tir, cast_type, (double) operand_c.i);
            }
            case TIR_FTOI: {
                int64_t int_size = sizeof_type(c->tir, cast_type, c->options->target);
                int64_t r = truncate_int_const((int64_t) operand_c.f, int_size * 8);
                return new_int_constant(c->tir, cast_type, r);
            }
            case TIR_ITRUNC: {
                int64_t int_size = sizeof_type(c->tir, cast_type, c->options->target);
                int64_t r = truncate_int_const(operand_c.i, int_size * 8);
                return new_int_constant(c->tir, cast_type, r);
            }
            case TIR_SEXT: {
                return new_int_constant(c->tir, cast_type, operand_c.i);
            }
            case TIR_FTRUNC:
            case TIR_FEXT: {
                return new_float_constant(c->tir, cast_type, operand_c.f);
            }
            default: {
                break;
            }
        }
    }

    return new_unary_inst(c, cast_kind, node, cast_type, operand_value.id);
}

static ValueId analyze_struct_ctor(TypeContext *c, AstId node, TypeId struct_type) {
    AstCall call = get_ast_call(node, c->ast);
    StructType s = get_struct_type(c->tir, struct_type);
    ValueId *args_tir = arena_alloc(c->scratch, ValueId, call.arg_count);
    TypeId *type_args = arena_alloc(c->scratch, TypeId, s.type_param_count);
    bool type_args_inferred = true;

    for (int32_t i = 0; i < call.arg_count; i++) {
        TypeId field_type = get_struct_type_field(c->tir, struct_type, i);
        if (s.type_param_count) {
            ValueId arg_result = analyze_value(c, call.args[i], field_type);
            TypeId arg_type = get_value_type(c->tir, arg_result);
            if (!match_type_parameters(c->tir, type_args, field_type, arg_type)) {
                type_args_inferred = false;
            }
            args_tir[i] = arg_result;
        } else {
            args_tir[i] = expect_value_type(c, call.args[i], field_type);
        }
    }

    if (type_args_inferred && s.type_param_count) {
        for (int32_t i = 0; i < call.arg_count; i++) {
            TypeId field_type = get_struct_type_field(c->tir, struct_type, i);
            field_type = replace_type_parameters(c->tir, type_args, field_type, *c->scratch);
            args_tir[i] = apply_implicit_conversion(c, call.args[i], args_tir[i], field_type);
        }
    }

    int32_t field_count = get_struct_type(c->tir, struct_type).field_count;
    if (field_count != call.arg_count) {
        type_error(c, call.operand, struct_type, call.arg_count, ERROR_FIELD_COUNT);
    }

    if (!type_args_inferred) {
        type_error(c, call.operand, struct_type, call.arg_count, ERROR_TYPE_ARGUMENT_INFERENCE);
        return null_value;
    }

    TypeId type = struct_type;
    if (s.type_param_count) {
        type = new_tagged_type(c->tir, struct_type, struct_type, s.type_param_count, type_args);
    }
    return new_binary_inst(c, TIR_NEW_STRUCT, node, type, push_extra(c, (int32_t *) args_tir, call.arg_count), call.arg_count);
}

static ValueId analyze_linear_ctor(TypeContext *c, AstId node, TypeId linear_type) {
    AstCall call = get_ast_call(node, c->ast);

    if (1 != call.arg_count) {
        type_error(c, call.operand, linear_type, 0, ERROR_LINEAR_CTOR_COUNT);
        return null_value;
    }

    TypeId param_type = get_linear_elem_type(c->tir, linear_type);
    ValueId arg_result = expect_value_type(c, get_call_arg(&call, 0), param_type);
    return new_unary_inst(c, TIR_NOP, node, linear_type, arg_result.id);
}

static ValueId analyze_constructor(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TypeId type = analyze_type(c, call.operand);
    switch (get_type_tag(c->tir, type)) {
        case TYPE_STRUCT: return analyze_struct_ctor(c, node, type);
        case TYPE_LINEAR: return analyze_linear_ctor(c, node, type);
        default: type_error(c, call.operand, type, 0, ERROR_TYPE_CONSTRUCTOR_TYPE); return null_value;
    }
}

static ValueId analyze_call(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    ValueId operand_value = analyze_value(c, call.operand, null_type);
    TypeId operand_type = get_value_type(c->tir, operand_value);

    if (get_type_tag(c->tir, operand_type) != TYPE_FUNCTION) {
        type_error(c, call.operand, operand_type, 0, ERROR_CALLEE);
        return null_value;
    }

    FunctionType func_type = get_function_type(c->tir, operand_type);
    ValueId *args_tir = arena_alloc(c->scratch, ValueId, call.arg_count);
    TypeId *type_args = arena_alloc(c->scratch, TypeId, func_type.type_param_count);
    bool type_args_inferred = true;

    for (int32_t i = 0; i < call.arg_count; i++) {
        TypeId param_type = get_function_type_param(c->tir, operand_type, i);
        if (func_type.type_param_count) {
            ValueId arg_result = analyze_value(c, call.args[i], param_type);
            TypeId arg_type = get_value_type(c->tir, arg_result);
            if (!match_type_parameters(c->tir, type_args, param_type, arg_type)) {
                type_args_inferred = false;
            }
            args_tir[i] = arg_result;
        } else {
            args_tir[i] = expect_value_type(c, call.args[i], param_type);
        }
    }

    if (type_args_inferred && func_type.type_param_count) {
        for (int32_t i = 0; i < call.arg_count; i++) {
            TypeId param_type = get_function_type_param(c->tir, operand_type, i);
            param_type = replace_type_parameters(c->tir, type_args, param_type, *c->scratch);
            args_tir[i] = apply_implicit_conversion(c, call.args[i], args_tir[i], param_type);
        }
    }

    if (func_type.param_count != call.arg_count) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_ARGUMENT_COUNT);
        return null_value;
    }

    if (!type_args_inferred) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_TYPE_ARGUMENT_INFERENCE);
        return null_value;
    }

    TypeId result_type = func_type.ret;
    if (func_type.type_param_count) {
        result_type = replace_type_parameters(c->tir, type_args, func_type.ret, *c->scratch);
    }

    return new_binary_inst(
        c,
        TIR_CALL,
        node,
        result_type,
        operand_value.id,
        push_extra(c, (int32_t *) args_tir, call.arg_count)
    );
}

static ValueId analyze_value_builtin_call(TypeContext *c, AstId node, TypeId hint) {
    switch ((BuiltinId) get_rir_data(node, c->rir)) {
        case BUILTIN_ALIGNOF: return analyze_alignof(c, node);
        case BUILTIN_SIZEOF: return analyze_sizeof(c, node);
        case BUILTIN_ZERO_EXTEND: return analyze_zero_extend(c, node, hint);
        case BUILTIN_SLICE: return analyze_slice_constructor(c, node, hint);
        default: abort();
    }
}

static TypeId analyze_type_builtin_call(TypeContext *c, AstId node) {
    switch ((BuiltinId) get_rir_data(node, c->rir)) {
        case BUILTIN_AFFINE: return analyze_linear(c, node);
        case BUILTIN_ARRAY_LENGTH_TYPE: return analyze_array_length_type(c, node);
        default: abort();
    }
}

static TypeId analyze_tagged_type(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TypeId newtype = analyze_type(c, call.operand);
    TypeId *arg_types = arena_alloc(c->scratch, TypeId, call.arg_count);
    for (int32_t i = 0; i < call.arg_count; i++) {
        arg_types[i] = analyze_type(c, call.args[i]);
    }
    if (!newtype.id) {
        return null_type;
    }
    NewtypeType n = get_newtype_type(c->tir, newtype);
    if (n.tags != call.arg_count) {
        type_error(c, call.operand, newtype, call.arg_count, ERROR_ARGUMENT_COUNT);
        return null_type;
    }
    return new_tagged_type(c->tir, newtype, n.type, call.arg_count, arg_types);
}

static TirId analyze_type_statement(TypeContext *c, AstId node) {
    analyze_type(c, get_ast_unary(node, c->ast));
    return null_tir;
}

static ValueId analyze_index(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    ValueId operand_value = analyze_value(c, call.operand, null_type);
    operand_value = implicit_pointer_deref(c, node, operand_value);
    TypeId operand_type = get_value_type(c->tir, operand_value);
    bool error = false;

    if (!remove_array_like(c->tir, operand_type).id) {
        type_error(c, call.operand, operand_type, 0, ERROR_INDEX_OPERAND);
        error = true;
    }

    if (call.arg_count != 1) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_INDEX_COUNT);
        return null_value;
    }

    ValueId arg_result = expect_value_type(c, call.args[0], type_isize);

    TypeId elem_type = remove_c_pointer_like(c->tir, operand_type);
    if (type_is_unknown_size(c->tir, elem_type)) {
        type_error(c, call.operand, elem_type, 0, ERROR_INDEX_UNKNOWN_TYPE_SIZE);
    }

    if (error) {
        return null_value;
    }

    return new_binary_inst(c, TIR_INDEX, node, elem_type, operand_value.id, arg_result.id);
}

static ValueId analyze_slice(TypeContext *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    ValueId operand_value = analyze_value(c, call.operand, null_type);
    TypeId operand_type = get_value_type(c->tir, operand_value);
    TypeId elem_type = remove_array_like(c->tir, operand_type);

    if (!elem_type.id) {
        type_error(c, call.operand, elem_type, 0, ERROR_INDEX_OPERAND);
        return null_value;
    }

    TypeId type;
    if (remove_slice(c->tir, operand_type).id) {
        type = operand_type;
    } else if (get_type_tag(c->tir, operand_type) == TYPE_ARRAY
        && get_value_category(c->tir, operand_value) == VALUE_MUTABLE_PLACE)
    {
        type = new_multiptr_type(c->tir, TYPE_MULTIPTR_MUT, elem_type);
    } else {
        type = new_multiptr_type(c->tir, TYPE_MULTIPTR, elem_type);
    }

    ValueId low_result;
    if (!is_ast_null(get_call_arg(&call, 0))) {
        low_result = expect_value_type(c, get_call_arg(&call, 0), type_isize);
    } else {
        low_result = new_int_constant(c->tir, type_isize, 0);
    }

    ValueId high_result;
    if (!is_ast_null(get_call_arg(&call, 1))) {
        high_result = expect_value_type(c, get_call_arg(&call, 1), type_isize);
    } else {
        high_result = resolve_length(c, node, operand_value);
    }

    int32_t extra[] = {
        low_result.id,
        high_result.id,
    };
    return new_binary_inst(
        c,
        TIR_SLICE,
        node,
        type,
        operand_value.id,
        push_extra(c, extra, ArrayLength(extra))
    );
}

static ValueId analyze_list(TypeContext *c, AstId node, TypeId hint) {
    AstList list = get_ast_list(node, c->ast);
    TypeId elem_type = remove_c_pointer_like(c->tir, hint);

    int32_t *args_tir = arena_alloc(c->scratch, int32_t, list.count);
    int32_t index = 0;

    for (int32_t i = 0; i < list.count; i++) {
        if (elem_type.id) {
            ValueId arg_result = expect_value_type(c, list.nodes[i], elem_type);
            args_tir[index++] = arg_result.id;
        } else {
            ValueId arg_result = analyze_value(c, list.nodes[i], null_type);
            elem_type = get_value_type(c->tir, arg_result);
            args_tir[index++] = arg_result.id;
        }
    }

    if (!list.count) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EMPTY_ARRAY});
        return null_value;
    }

    TypeId type = new_array_type(c->tir, new_array_length_type(c->tir, list.count), elem_type);
    return new_binary_inst(c, TIR_NEW_ARRAY, node, type, push_extra(c, args_tir, list.count), list.count);
}

static TirId analyze_if(TypeContext *c, AstId node) {
    AstIf if_ = get_ast_if(node, c->ast);
    ValueId cond_result = expect_value_type(c, if_.condition, type_bool);
    TirBlock true_tir = analyze_block(c, if_.true_block);
    TirBlock false_tir = {0};
    if (!is_ast_null(if_.false_block)) {
        false_tir = analyze_block(c, if_.false_block);
    }
    int32_t extra[] = {
        true_tir.index,
        true_tir.length,
        false_tir.index,
        false_tir.length,
    };
    return new_binary_statement(c, TIR_IF, node, cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_while(TypeContext *c, AstId node) {
    AstBinary while_ = get_ast_binary(node, c->ast);
    c->loop_depth++;
    ValueId cond_result = expect_value_type(c, while_.left, type_bool);
    TirBlock block_tir = analyze_block(c, while_.right);
    c->loop_depth--;
    int32_t extra[] = {
        0,
        block_tir.index,
        block_tir.length,
    };
    return new_binary_statement(c, TIR_LOOP, node, cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_for_helper(TypeContext *c, AstId node) {
    AstFor for_ = get_ast_for(get_ast_unary(node, c->ast), c->ast);
    return analyze_statement(c, for_.init);
}

static TirId analyze_for(TypeContext *c, AstId node) {
    AstFor for_ = get_ast_for(node, c->ast);
    c->loop_depth++;
    ValueId cond_result = expect_value_type(c, for_.condition, type_bool);
    TirBlock block_tir = analyze_block(c, for_.block);
    ValueId next_tir = analyze_value(c, for_.next, null_type);
    c->loop_depth--;
    int32_t extra[] = {
        next_tir.id,
        block_tir.index,
        block_tir.length,
    };
    return new_binary_statement(c, TIR_LOOP, node, cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static void validate_exhaustive_enum_switch(TypeContext *c, AstId node, EnumType *type, int32_t *branches) {
    AstCall switch_ = get_ast_call(node, c->ast);
    HashTable const *scope = &c->global->type_scopes.ptr[type->scope];
    bool *seen_enum_values = arena_alloc(c->scratch, bool, scope->count);
    AstId else_case = null_ast;

    for (int32_t i = 0; i < switch_.arg_count; i++) {
        AstBinary branch = get_ast_binary(switch_.args[i], c->ast);

        if (is_ast_null(branch.left)) {
            else_case = switch_.args[i];
            continue;
        }

        int64_t value = 0;
        try_get_int_const(c, (ValueId) {branches[i * 2]}, &value);
        if (seen_enum_values[value]) {
            error(c, branch.left, &(Diagnostic) {.kind = ERROR_DUPLICATE_SWITCH_CASE});
        } else {
            seen_enum_values[value] = true;
        }
    }

    bool has_all = true;
    for (size_t i = 0; i < scope->count; i++) {
        if (!seen_enum_values[i]) {
            has_all = false;
            break;
        }
    }

    if (has_all && !is_ast_null(else_case)) {
        error(c, else_case, &(Diagnostic) {.kind = ERROR_ELSE_CASE_UNREACHABLE});
    }

    if (!has_all && is_ast_null(else_case)) {
        error(c, node, &(Diagnostic) {.kind = ERROR_SWITCH_NOT_EXHAUSTIVE});
    }
}

static ValueId analyze_switch(TypeContext *c, AstId node, TypeId hint) {
    AstCall switch_ = get_ast_call(node, c->ast);
    TypeId pattern_type = type_bool;
    ValueId cond_value = null_value;

    if (!is_ast_null(switch_.operand)) {
        ValueId cond_result = analyze_value(c, switch_.operand, null_type);
        pattern_type = get_value_type(c->tir, cond_result);
        cond_value = cond_result;
    }

    int32_t *branches_tir = arena_alloc(c->scratch, int32_t, switch_.arg_count * 2);
    TypeId result_type = hint;
    bool consistent_types = true;
    AstId first_incompatible_case = node;
    AstId else_case = null_ast;

    for (int32_t i = 0; i < switch_.arg_count; i++) {
        AstBinary branch = get_ast_binary(switch_.args[i], c->ast);

        if (!is_ast_null(branch.left)) {
            ValueId pattern_result = expect_value_type(c, branch.left, pattern_type);
            branches_tir[i * 2] = pattern_result.id;
        } else {
            branches_tir[i * 2] = 0;
            else_case = switch_.args[i];
        }

        ValueId value_result = expect_value_type(c, branch.right, hint);
        branches_tir[i * 2 + 1] = value_result.id;

        if (!result_type.id && consistent_types) {
            result_type = get_value_type(c->tir, value_result);
        }

        if (!value_result.id) {
            consistent_types = false;
        }

        if (pattern_type.id && get_value_type(c->tir, value_result).id != result_type.id) {
            first_incompatible_case = switch_.args[i];
            consistent_types = false;
        }
    }

    if (result_type.id) {
        if (!consistent_types) {
            error(c, first_incompatible_case, &(Diagnostic) {.kind = ERROR_SWITCH_INCOMPATIBLE_CASES});
        } else {
            if (result_type.id != TYPE_VOID) {
                if (get_type_tag(c->tir, pattern_type) == TYPE_ENUM) {
                    EnumType enum_type = get_enum_type(c->tir, pattern_type);
                    validate_exhaustive_enum_switch(c, node, &enum_type, branches_tir);
                } else if (is_ast_null(else_case)) {
                    error(c, node, &(Diagnostic) {.kind = ERROR_SWITCH_NOT_EXHAUSTIVE});
                }
            }
        }

        if (consistent_types) {
            int32_t extra[] = {
                push_extra(c, branches_tir, switch_.arg_count * 2),
                switch_.arg_count,
            };
            return new_binary_inst(c, TIR_SWITCH, node, result_type, cond_value.id, push_extra(c, extra, ArrayLength(extra)));
        }
    }

    return null_value;
}

static TirId analyze_break(TypeContext *c, AstId node) {
    if (!c->loop_depth) {
        error(c, node, &(Diagnostic) {.kind = ERROR_MISPLACED_BREAK});
    }

    return new_binary_statement(c, TIR_BREAK, node, 0, 0);
}

static TirId analyze_continue(TypeContext *c, AstId node) {
    if (!c->loop_depth) {
        error(c, node, &(Diagnostic) {.kind = ERROR_MISPLACED_CONTINUE});
    }

    return new_binary_statement(c, TIR_CONTINUE, node, 0, 0);
}

static TirId analyze_return(TypeContext *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);

    if (!c->current_function_type.id) {
        compiler_error("return statement outside of function");
    }

    FunctionType func_type = get_function_type(c->tir, c->current_function_type);

    if (!is_ast_null(operand)) {
        TypeId operand_hint = func_type.ret;

        if (func_type.ret.id == TYPE_VOID) {
            error(c, operand, &(Diagnostic) {.kind = ERROR_RETURN_EXPECTED_VALUE});
            operand_hint = null_type;
        }

        ValueId operand_value = expect_value_type(c, operand, operand_hint);
        return new_binary_statement(c, TIR_RETURN, node, operand_value.id, 0);
    } else {
        if (func_type.ret.id != TYPE_VOID) {
            error(c, operand, &(Diagnostic) {.kind = ERROR_RETURN_MISSING_VALUE});
        }

        return new_binary_statement(c, TIR_RETURN, node, 0, 0);
    }
}

static TirId analyze_value_statement(TypeContext *c, AstId node) {
    return new_binary_statement(c, TIR_VALUE, node, analyze_value(c, get_ast_unary(node, c->ast), null_type).id, 0);
}

static ValueId analyze_value(TypeContext *c, AstId node, TypeId hint) {
    switch (get_rir_tag(node, c->rir)) {
        case RIR_BUILTIN_ID: return analyze_value_id(c, node, SYM_BUILTIN);
        case RIR_GLOBAL_ID: return analyze_value_id(c, node, SYM_GLOBAL);
        case RIR_LOCAL_ID: return analyze_value_id(c, node, SYM_LOCAL);
        case RIR_INT: return analyze_int(c, node, hint);
        case RIR_FLOAT: return analyze_float(c, node, hint);
        case RIR_CHAR: return analyze_char(c, node);
        case RIR_STRING: return analyze_string(c, node);
        case RIR_BOOL: return analyze_bool(c, node);
        case RIR_NULL: return analyze_null(c, hint);
        case RIR_PLUS: return analyze_un_arithmetic(c, node, hint, TIR_PLUS);
        case RIR_MINUS: return analyze_un_arithmetic(c, node, hint, TIR_MINUS);
        case RIR_NOT: return analyze_not(c, node);
        case RIR_ADDRESS: return analyze_address(c, node, hint);
        case RIR_MULTIADDRESS: return analyze_multiaddress(c, node, hint);
        case RIR_DEREF: return analyze_deref(c, node);
        case RIR_ADD: return analyze_bin_arithmetic(c, node, hint, TIR_ADD);
        case RIR_SUB: return analyze_bin_arithmetic(c, node, hint, TIR_SUB);
        case RIR_MUL: return analyze_bin_arithmetic(c, node, hint, TIR_MUL);
        case RIR_DIV: return analyze_bin_arithmetic(c, node, hint, TIR_DIV);
        case RIR_MOD: return analyze_bin_arithmetic(c, node, hint, TIR_MOD);
        case RIR_AND: return analyze_bin_bit(c, node, hint, TIR_AND);
        case RIR_OR: return analyze_bin_bit(c, node, hint, TIR_OR);
        case RIR_XOR: return analyze_bin_bit(c, node, hint, TIR_XOR);
        case RIR_SHL: return analyze_bin_bit(c, node, hint, TIR_SHL);
        case RIR_SHR: return analyze_bin_bit(c, node, hint, TIR_SHR);
        case RIR_LOGIC_AND: return analyze_logic(c, node, true);
        case RIR_LOGIC_OR: return analyze_logic(c, node, false);
        case RIR_EQ: return analyze_eq(c, node, TIR_EQ);
        case RIR_NE: return analyze_eq(c, node, TIR_NE);
        case RIR_LT: return analyze_rel(c, node, TIR_LT);
        case RIR_GT: return analyze_rel(c, node, TIR_GT);
        case RIR_LE: return analyze_rel(c, node, TIR_LE);
        case RIR_GE: return analyze_rel(c, node, TIR_GE);
        case RIR_ASSIGN: return analyze_assign(c, node);
        case RIR_ASSIGN_ADD: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_ADD);
        case RIR_ASSIGN_SUB: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_SUB);
        case RIR_ASSIGN_MUL: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_MUL);
        case RIR_ASSIGN_DIV: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_DIV);
        case RIR_ASSIGN_MOD: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_MOD);
        case RIR_ASSIGN_AND: return analyze_assign_bit(c, node, TIR_ASSIGN_AND);
        case RIR_ASSIGN_OR: return analyze_assign_bit(c, node, TIR_ASSIGN_OR);
        case RIR_ASSIGN_XOR: return analyze_assign_bit(c, node, TIR_ASSIGN_XOR);
        case RIR_TYPE_ACCESS: return analyze_struct_access(c, node);
        case RIR_SCOPE_ACCESS: return analyze_enum_member(c, node);
        case RIR_INFERRED_SCOPE_ACCESS: return analyze_enum_member_inferred(c, node, hint);
        case RIR_CAST: return analyze_cast(c, node);
        case RIR_CALL: return analyze_call(c, node);
        case RIR_CONSTRUCT: return analyze_constructor(c, node);
        case RIR_CALL_BUILTIN: return analyze_value_builtin_call(c, node, hint);
        case RIR_INDEX: return analyze_index(c, node);
        case RIR_SLICE: return analyze_slice(c, node);
        case RIR_LIST: return analyze_list(c, node, hint);
        case RIR_SWITCH: return analyze_switch(c, node, hint);
        default: return null_value;
    }
}

static TypeId analyze_type(TypeContext *c, AstId node) {
    switch (get_rir_tag(node, c->rir)) {
        case RIR_BUILTIN_ID: return analyze_type_id(c, node, SYM_BUILTIN);
        case RIR_GLOBAL_ID: return analyze_type_id(c, node, SYM_GLOBAL);
        case RIR_LOCAL_ID: return analyze_type_id(c, node, SYM_LOCAL);
        case RIR_ARRAY_TYPE: return analyze_array_type(c, node);
        case RIR_ARRAY_TYPE_SUGAR: return analyze_array_type_sugar(c, node);
        case RIR_POINTER_TYPE: return analyze_ptr(c, node, TYPE_PTR);
        case RIR_MUTABLE_POINTER_TYPE: return analyze_ptr(c, node, TYPE_PTR_MUT);
        case RIR_SLICE_TYPE: return analyze_multiptr(c, node, TYPE_MULTIPTR);
        case RIR_MUTABLE_SLICE_TYPE: return analyze_multiptr(c, node, TYPE_MULTIPTR_MUT);
        case RIR_FUNCTION_TYPE: return analyze_function_type(c, node);
        case RIR_TAG_TYPE: return analyze_tagged_type(c, node);
        case RIR_CALL_BUILTIN: return analyze_type_builtin_call(c, node);
        default: return null_type;
    }
}

static TirId analyze_statement(TypeContext *c, AstId node) {
    switch (get_rir_tag(node, c->rir)) {
        case RIR_LET: return analyze_let(c, node, false);
        case RIR_MUT: return analyze_let(c, node, true);
        case RIR_TYPE_ALIAS: c->local->tir_refs[get_rir_data(node, c->rir)] = analyze_type_alias(c, node); return null_tir;
        case RIR_CONST: c->local->tir_refs[get_rir_data(node, c->rir)] = analyze_const(c, node); return null_tir;
        case RIR_IF: return analyze_if(c, node);
        case RIR_WHILE: return analyze_while(c, node);
        case RIR_FOR_HELPER: return analyze_for_helper(c, node);
        case RIR_FOR: return analyze_for(c, node);
        case RIR_BREAK: return analyze_break(c, node);
        case RIR_CONTINUE: return analyze_continue(c, node);
        case RIR_RETURN: return analyze_return(c, node);
        case RIR_VALUE_STATEMENT: return analyze_value_statement(c, node);
        case RIR_TYPE_STATEMENT: return analyze_type_statement(c, node);
        default: return null_tir;
    }
}

static void analyze_def(TypeContext *c, DefId def) {
    AstId node = c->ast_refs[def.id].node;
    TirRef ref = {0};
    switch (get_rir_tag(node, c->rir)) {
        case RIR_FUNCTION: ref = analyze_function_decl(c, node); break;
        case RIR_ENUM: ref = analyze_enum(c, node); break;
        case RIR_STRUCT: ref = analyze_struct(c, node); break;
        case RIR_NEWTYPE: ref = analyze_newtype(c, node); break;
        case RIR_EXTERN_FUNCTION: ref = analyze_extern_function(c, node); break;
        case RIR_EXTERN_MUT: ref = analyze_extern_mut(c, node); break;
        case RIR_TYPE_ALIAS: ref = analyze_type_alias(c, node); break;
        case RIR_CONST: ref = analyze_const(c, node); break;
        default: break;
    }
    c->tir_refs[def.id] = ref;
}

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch) {
    GlobalData global = {0};
    TirDependencies global_tir = {0};
    init_tir_deps(&global_tir);
    TypeContext global_tc = {0};
    global_tc.options = input->options;
    global_tc.paths = input->paths;
    global_tc.sources = input->sources;
    global_tc.asts = input->asts;
    global_tc.permanent = permanent;
    global_tc.scratch = &scratch;
    global_tc.ast_refs = input->ast_refs;
    global_tc.tir_refs = arena_alloc(&scratch, TirRef, input->def_count);
    global_tc.global = &global;
    global_tc.tir.global = &global_tir;
    LocalData *local_data = arena_alloc(&scratch, LocalData, input->def_count);

    for (int32_t i = 0; i < input->order_count; i++) {
        DefId def = input->order[i];
        global_tc.file = global_tc.ast_refs[def.id].file,
        global_tc.local_ast_refs = &global_tc.local_ast_refs[global_tc.file],
        global_tc.ast = &input->asts[global_tc.file];
        global_tc.rir = &input->rirs[global_tc.file];

        local_data[def.id].tir_refs = arena_alloc(&scratch, TirRef, input->local_ast_refs[global_tc.file].len);
        local_data[def.id].notes_shown = arena_alloc(&scratch, bool, input->local_ast_refs[global_tc.file].len);
        global_tc.local = &local_data[def.id];
        analyze_def(&global_tc, def);
    }

    LocalTir *tirs = arena_alloc(permanent, LocalTir, input->function_count);
    int err = global_tc.error;

    #pragma omp parallel
    {
        Arena thread_base_scratch = new_arena(64 << 20);
        Arena thread_scratch = thread_base_scratch;
        TypeContext local_tc = {0};
        local_tc.options = global_tc.options;
        local_tc.paths = global_tc.paths;
        local_tc.sources = global_tc.sources;
        local_tc.asts = global_tc.asts;
        local_tc.permanent = permanent;
        local_tc.scratch = &thread_scratch;
        local_tc.ast_refs = global_tc.ast_refs;
        local_tc.tir_refs = global_tc.tir_refs;
        local_tc.global = &global;
        local_tc.tir.global = &global_tir;

        #pragma omp for reduction (||:err)
        for (int32_t i = 0; i < input->function_count; i++) {
            DefId def = input->functions[i];
            ValueId value = global_tc.tir_refs[def.id].value;
            AstRef ref = input->ast_refs[def.id];

            local_tc.local = &local_data[def.id];

            local_tc.file = ref.file;
            local_tc.local_ast_refs = &input->local_ast_refs[ref.file],
            local_tc.ast = &input->asts[ref.file];
            local_tc.rir = &input->rirs[ref.file];
            local_tc.tir.thread = &tirs[i];

            // Add null tir
            new_inst_impl(&local_tc, TIR_NOP, null_ast, 0, 0);

            tirs[i].first = analyze_function(&local_tc, ref.node, value);

            if (local_tc.error) {
                err = 1;
            }
        }

        delete_arena(&thread_base_scratch);
    }

    return (TirOutput) {
        .declarations = global.declarations,
        .global_deps = global_tir,
        .insts = tirs,
        .error = err,
    };
}
