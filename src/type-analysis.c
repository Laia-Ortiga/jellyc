#include "type-analysis.h"

#include "adt.h"
#include "arena.h"
#include "data/ast.h"
#include "data/tir.h"
#include "diagnostic.h"
#include "fwd.h"
#include "hash.h"
#include "lex.h"
#include "util.h"

#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Scope {
    struct Scope *parent;
    HashTable table;
} Scope;

typedef enum {
    ROLE_NOT_VISITED,
    ROLE_VISITING,
    ROLE_VISITED,
} Role;

typedef struct {
    AstId node;
    TirId tir_ref;
    bool notes_shown;  // Whether a note to fix an error has been shown already.
} Local;

typedef struct {
    Options *options;
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    HashTable *module_table;
    Module *modules;
    HashTable *global_scope;
    AstRef *ast_refs;

    bool *module_import_notes;
    Scope *scope;

    Arena *permanent;
    Arena *scratch;

    Role *rirs;
    TirId *tir_refs;

    int error;

    int32_t file;
    Ast *ast;

    Vec(Local) locals;

    TirContext tir;
    LocalTir *local_tirs;
    LocalTir *local_tir;
    TirId current_function_type;
    int32_t loop_depth;
} Context;

static void push_scope(Context *c) {
    Scope *parent = c->scope;
    c->scope = arena_alloc(c->scratch, Scope, 1);
    c->scope->parent = parent;
    c->scope->table = htable_init();
}

static void pop_scope(Context *c) {
    htable_free(&c->scope->table);
    c->scope = c->scope->parent;
}

static Symbol lookup(Context *c, int32_t file, String name) {
    int32_t *file_def = htable_lookup(&c->files[file].scope, name);
    if (file_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*file_def}};
    }

    int32_t module = c->files[file].module;

    int32_t *private_def = htable_lookup(&c->modules[module].private_scope, name);
    if (private_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*private_def}};
    }

    int32_t *public_def = htable_lookup(&c->modules[module].public_scope, name);
    if (public_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*public_def}};
    }

    int32_t *builtin_def = htable_lookup(c->global_scope, name);
    if (builtin_def) {
        if (*builtin_def >= TERM_COUNT) {
            return (Symbol) {.kind = SYM_GLOBAL, .global = {*builtin_def - TERM_COUNT}};
        }
        return (Symbol) {.kind = SYM_BUILTIN, .builtin = *builtin_def};
    }

    return (Symbol) {0};
}

static String get_id_source(Context const *c, AstRef ref) {
    SourceIndex token = get_ast_token(ref.node, &c->asts[ref.file]);
    return id_token_to_string(c->sources[ref.file], token);
}

static void diagnostic(Context *c, AstRef ref, ErrorKind kind) {
    SourceIndex token = get_ast_token(ref.node, &c->asts[ref.file]);
    String name = id_token_to_string(c->sources[ref.file], token);
    SourceLoc loc = {
        .path = c->paths[ref.file],
        .source = c->sources[ref.file],
        .where = token,
        .len = name.len,
        .mark = token,
    };
    print_diagnostic(&loc, &(Diagnostic) {.kind = kind});
    c->error = 1;
}

static LocalId lookup_local(Context *c, String name) {
    for (Scope *scope = c->scope; scope; scope = scope->parent) {
        int32_t *symbol = htable_lookup(&scope->table, name);
        if (symbol) {
            return (LocalId) {*symbol};
        }
    }
    return (LocalId) {0};
}

static Symbol find_symbol(Context *c, int32_t file, String name) {
    LocalId local = lookup_local(c, name);
    if (local.id) {
        return (Symbol) {.kind = SYM_LOCAL, .local = local};
    }
    return lookup(c, file, name);
}

static SourceLoc get_ast_location(Context *c, AstRef def) {
    SourceIndex token = get_ast_token(def.node, &c->asts[def.file]);
    String name = id_token_to_string(c->sources[def.file], token);
    return (SourceLoc) {
        .path = c->paths[def.file],
        .source = c->sources[def.file],
        .where = token,
        .len = name.len,
        .mark = token,
    };
}

static void add_id(Context *c, AstRef ref, TirId term) {
    String name = get_id_source(c, ref);
    Symbol prev_symbol = find_symbol(c, ref.file, name);

    if (prev_symbol.kind == SYM_GLOBAL
        && c->rirs[prev_symbol.global.id] == ROLE_VISITING) {
        c->tir_refs[prev_symbol.global.id] = term;
        return;
    }

    if (prev_symbol.kind != SYM_UNDEFINED) {
        diagnostic(c, ref, ERROR_MULTIPLE_DEFINITION);
        AstRef prev_ref;
        switch (prev_symbol.kind) {
            case SYM_BUILTIN: {
                SourceLoc loc = get_ast_location(c, ref);
                print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_BUILTIN_DEFINITION});
                return;
            }
            case SYM_GLOBAL: {
                prev_ref = c->ast_refs[prev_symbol.global.id];
                break;
            }
            case SYM_LOCAL: {
                Local info = c->locals.ptr[prev_symbol.local.id - 1];
                prev_ref = (AstRef) {info.node, ref.file};
                break;
            }
            default: {
                return;
            }
        }
        SourceLoc loc = get_ast_location(c, prev_ref);
        print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
        return;
    }

    if (!c->scope) {
        compiler_error("no local scope");
    }
    int32_t sym = c->locals.len + 1;
    Local local_ref = {ref.node, term, false};
    vec_push(&c->locals, local_ref);
    htable_try_insert(&c->scope->table, name, sym);
}

static int32_t push_extra(Context *c, int32_t *values, int32_t count) {
    int32_t index = c->tir.thread->terms.extra.len;
    int32_t *result = vec_grow(&c->tir.thread->terms.extra, count);
    for (int32_t i = 0; i < count; i++) {
        result[i] = values[i];
    }
    return index;
}

static String ctx_source(Context const *c) {
    return c->sources[c->file];
}

static SourceLoc ctx_init_loc(Context const *c, SourceIndex start, ptrdiff_t len) {
    SourceLoc loc = {
        .path = c->paths[c->file],
        .source = c->sources[c->file],
        .where = start,
        .len = len,
        .mark = start,
    };
    return loc;
}

static void add_error(Context *c) {
    c->error = 1;
}

static void error(Context *c, AstId child, Diagnostic const *diagnostic) {
    SourceIndex child_token = get_ast_token(child, c->ast);
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

static void type_error(Context *c, AstId child, TirId type, int32_t extra, ErrorKind kind) {
    if (!type.id) {
        return;
    }

    SourceIndex child_token = get_ast_token(child, c->ast);
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

static void double_type_error(Context *c, AstId child, TirId type1, TirId type2, ErrorKind kind) {
    if (!type1.id) {
        return;
    }

    if (!type2.id) {
        return;
    }

    SourceIndex child_token = get_ast_token(child, c->ast);
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

static int32_t ctx_push_str(Context const *c, String s) {
    char null = '\0';

    if (c->tir.thread) {
        int32_t index = push_str(&c->tir.thread->strtab, s);
        push_str(&c->tir.thread->strtab, (String) {1, &null});
        return index;
    }

    int32_t index = push_str(&c->tir.global->strtab, s);
    push_str(&c->tir.global->strtab, (String) {1, &null});
    return index;
}

static int32_t ctx_push_double_str(Context const *c, String s1, String s2) {
    char null = '\0';

    if (c->tir.thread) {
        int32_t index = push_str(&c->tir.thread->strtab, s1);
        push_str(&c->tir.thread->strtab, s2);
        push_str(&c->tir.thread->strtab, (String) {1, &null});
        return index;
    }

    int32_t index = push_str(&c->tir.global->strtab, s1);
    push_str(&c->tir.global->strtab, s2);
    push_str(&c->tir.global->strtab, (String) {1, &null});
    return index;
}

// Analysis

static TirId analyze_term(Context *c, AstId node, TirId hint);

static int analyze_def(Context *c, DefId def) {
    Role prev_role = c->rirs[def.id];
    if (prev_role == ROLE_VISITED) {
        return 0;
    }
    AstRef ref = c->ast_refs[def.id];
    if (prev_role == ROLE_VISITING) {
        diagnostic(c, ref, ERROR_RECURSIVE_DEPENDENCY);
        return 1;
    }
    c->rirs[def.id] = ROLE_VISITING;
    Context new_c = *c;
    new_c.file = ref.file;
    new_c.ast = &c->asts[ref.file];
    new_c.scope = NULL;
    new_c.loop_depth = 0;
    new_c.current_function_type = null_tir;
    new_c.tir.thread = NULL;
    analyze_term(&new_c, ref.node, null_tir);
    c->rirs[def.id] = ROLE_VISITED;
    return 0;
}

static TirId resolve_global(Context *c, AstRef ref, DefId global) {
    if (analyze_def(c, global)) {
        diagnostic(c, ref, NOTE_RECURSION);
    }
    return c->tir_refs[global.id];
}

static TirId expect_mutable_place(Context *c, AstId node, TirId hint) {
    TirId result = analyze_term(c, node, hint);
    switch (get_value_category(c->tir, result)) {
        case VALUE_INVALID: return null_tir;
        case VALUE_MUTABLE_PLACE: return result;
        default: break;
    }

    error(c, node, &(Diagnostic) {.kind = ERROR_EXPECTED_MUTABLE_PLACE});

    if (get_term_tag(c->tir, result) == TIR_VARIABLE) {
        int32_t var_index = get_term_data(c->tir, result)->b;
        AstTag tag = get_ast_tag(get_term_data(c->tir, result)->node, c->ast);

        if (!c->locals.ptr[var_index].notes_shown && tag == AST_LET) {
            c->locals.ptr[var_index].notes_shown = true;
            SourceIndex token = get_ast_token(get_term_data(c->tir, result)->node, c->ast);
            String name = id_token_to_string(ctx_source(c), token);
            SourceLoc note = ctx_init_loc(c, token, name.len);
            print_diagnostic(&note, &(Diagnostic) {.kind = NOTE_REPLACE_LET_WITH_MUT});
        }
    }

    return null_tir;
}

static TirId expect_type(Context *c, AstId node) {
    TirId result = analyze_term(c, node, null_tir);
    if (is_tir_type(get_term_tag(c->tir, result))) {
        return result;
    }

    if (result.id) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EXPECTED_TYPE});
    }

    return null_tir;
}

static TirId expect_value(Context *c, AstId node, TirId hint) {
    TirId result = analyze_term(c, node, hint);
    if (is_tir_value(get_term_tag(c->tir, result))) {
        return result;
    }

    if (result.id) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EXPECTED_VALUE});
    }

    return null_tir;
}

static TirId apply_implicit_conversion(Context *c, AstId node, TirId value, TirId wanted_type) {
    if (!wanted_type.id) {
        return value;
    }

    TirId provided = get_value_type(c->tir, value);
    if (provided.id == wanted_type.id) {
        return value;
    }

    // Types don't match. Attempt implicit conversion.
    TirId types[] = {provided, wanted_type};
    TirId t[2] = {0};

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
            return new_binary_tir(
                c->tir,
                TIR_ARRAY_TO_SLICE,
                node,
                wanted_type,
                value,
                t[1]
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
            return new_binary_tir(
                c->tir,
                TIR_ARRAY_TO_SLICE,
                node,
                wanted_type,
                value,
                t[1]
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
            return new_unary_tir(c->tir, TIR_NOP, node, wanted_type, value);
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
            return new_unary_tir(c->tir, TIR_NOP, node, wanted_type, value);
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
            return new_unary_tir(c->tir, TIR_PTR_CAST, node, wanted_type, value);
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
            return new_unary_tir(c->tir, TIR_PTR_CAST, node, wanted_type, value);
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
            return new_unary_tir(c->tir, TIR_NOP, node, wanted_type, value);
        }
    }

    double_type_error(c, node, wanted_type, provided, ERROR_EXPECTED_VALUE_TYPE);
    return null_tir;
}

static TirId expect_value_type(Context *c, AstId node, TirId wanted_type) {
    TirId result = expect_value(c, node, wanted_type);
    return apply_implicit_conversion(c, node, result, wanted_type);
}

static bool try_get_int_const(Context *c, TirId value, int64_t *i) {
    if (get_term_tag(c->tir, value) == TIR_CONST_INT) {
        *i = get_value_int(c->tir, value);
        return true;
    }

    return false;
}

static TirId analyze_return_type(Context *c, AstId node) {
    TirId ret = ptype(VOID);
    if (!is_ast_null(node)) {
        ret = expect_type(c, node);
        if (type_is_unknown_size(c->tir, ret)) {
            type_error(c, node, ret, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
        }
    }
    return ret;
}

static TirId analyze_import(Context *c, AstId node) {
    AstRef ref = {node, c->file};
    String name = get_id_source(c, ref);
    int32_t *module = htable_lookup(c->module_table, name);

    if (!module) {
        diagnostic(c, ref, ERROR_UNDEFINED_MODULE);
        return null_tir;
    }

    TirId m = {~*module};
    add_id(c, ref, m);
    return m;
}

static TirId analyze_function_decl(Context *c, AstId node) {
    AstFunction f = get_ast_function(node, c->ast);
    push_scope(c);

    TirId *type_param_types = arena_alloc(c->scratch, TirId, f.type_param_count);
    for (int32_t i = 0; i < f.type_param_count; i++) {
        SourceIndex token = get_ast_token(f.type_params[i], c->ast);
        String name = id_token_to_string(ctx_source(c), token);
        type_param_types[i] = new_type_parameter(c->tir, i, ctx_push_str(c, name));
        add_id(c, (AstRef) {f.type_params[i], c->file}, type_param_types[i]);
    }

    TirId *param_types = arena_alloc(c->scratch, TirId, f.param_count);
    for (int32_t i = 0; i < f.param_count; i++) {
        AstId param_type = get_ast_unary(f.params[i], c->ast);
        param_types[i] = expect_type(c, param_type);
        TirId param_value = new_variable(c->tir, f.params[i], param_types[i], i, false);
        add_id(c, (AstRef) {f.params[i], c->file}, param_value);
    }

    TirId ret_type = analyze_return_type(c, f.ret);
    TirId type = new_function_type(c->tir, &(FunctionType) {
        .param_count = f.param_count,
        .params = param_types,
        .ret = ret_type,
    });

    SourceIndex token = get_ast_token(node, c->ast);
    String name = id_token_to_string(ctx_source(c), token);
    char name_buffer[64];
    int length = snprintf(name_buffer, sizeof(name_buffer), "file%d_", c->file);
    pop_scope(c);
    c->locals.len = 0;
    TirId inner_value = new_function(c->tir, type, ctx_push_double_str(c, (String) {length, name_buffer}, name));
    TirId value = inner_value;

    if (f.type_param_count) {
        value = new_generic(c->tir, inner_value, f.type_param_count, type_param_types);
    }

    add_id(c, (AstRef) {node, c->file}, value);

    if (equals(name, (String) Str("main"))) {
        if (f.type_param_count || f.param_count || !is_ast_null(f.ret)) {
            error(c, node, &(Diagnostic) {.kind = ERROR_MAIN_SIGNATURE});
        }
        c->tir.global->main = inner_value;
    }

    vec_push(&c->tir.global->functions, inner_value);
    return value;
}

typedef struct {
    int32_t index;
    int32_t length;
} TirBlock;

static TirId analyze_return(Context *c, AstId operand) {
    if (!c->current_function_type.id) {
        compiler_error("return statement outside of function");
    }

    FunctionType func_type = get_function_type(c->tir, c->current_function_type);

    if (!is_ast_null(operand)) {
        TirId operand_hint = func_type.ret;

        if (func_type.ret.id == TYPE_VOID) {
            error(c, operand, &(Diagnostic) {.kind = ERROR_RETURN_EXPECTED_VALUE});
            operand_hint = null_tir;
        }

        TirId operand_value = expect_value_type(c, operand, operand_hint);
        return new_unary_tir(c->tir, TIR_RETURN, operand, null_tir, operand_value);
    } else {
        if (func_type.ret.id != TYPE_VOID) {
            error(c, operand, &(Diagnostic) {.kind = ERROR_RETURN_MISSING_VALUE});
        }

        return new_unary_tir(c->tir, TIR_RETURN, operand, null_tir, null_tir);
    }
}

static TirBlock analyze_block(Context *c, AstId block, TirId hint) {
    AstList list = get_ast_list(block, c->ast);
    int32_t *body_tir = arena_alloc(c->scratch, int32_t, list.count);
    int32_t length = 0;
    for (int32_t i = 0; i < list.count; i++) {
        TirId tir = null_tir;
        if (i == list.count - 1 && hint.id && hint.id != TYPE_VOID) {
            tir = analyze_return(c, list.nodes[i]);
        } else {
            tir = analyze_term(c, list.nodes[i], null_tir);
        }
        if (is_tir_value(get_term_tag(c->tir, tir))) {
            body_tir[length++] = tir.id;
        }
    }
    return (TirBlock) {push_extra(c, body_tir, length), length};
}

static void analyze_function(Context *c, AstId node, TirId value) {
    AstFunction f = get_ast_function(node, c->ast);
    GenericTerm g = get_generic_term(c->tir, value);
    TirId type = get_value_type(c->tir, g.inner);
    FunctionType func_type = get_function_type(c->tir, type);
    push_scope(c);
    for (int32_t i = 0; i < g.type_count; i++) {
        add_id(c, (AstRef) {f.type_params[i], c->file}, g.types[i]);
    }
    for (int32_t i = 0; i < func_type.param_count; i++) {
        TirId param_type = get_function_type_param(c->tir, type, i);
        int32_t var = c->local_tir->local_count++;
        TirId param_value = new_variable(c->tir, f.params[i], param_type, var, false);
        add_id(c, (AstRef) {f.params[i], c->file}, param_value);
    }
    c->current_function_type = type;
    TirBlock tir_block = analyze_block(c, f.body, func_type.ret);
    if (tir_block.length == 0 && func_type.ret.id != TYPE_VOID) {
        error(c, f.body, &(Diagnostic) {.kind = ERROR_MISSING_RETURN});
    }
    pop_scope(c);
    c->local_tir->body_first = tir_block.index;
    c->local_tir->body_length = tir_block.length;
}

static TirId analyze_enum(Context *c, AstId node) {
    AstEnum e = get_ast_enum(node, c->ast);

    TirId repr_type = expect_type(c, e.repr);
    if (!type_is_int(repr_type)) {
        type_error(c, e.repr, repr_type, 0, ERROR_ENUM_EXPECTS_INT_TYPE);
        repr_type = null_tir;
    }

    SourceIndex token = get_ast_token(node, c->ast);
    String name = id_token_to_string(ctx_source(c), token);
    HashTable table_init = htable_init();
    int32_t scope = c->tir.global->type_scopes.len;
    vec_push(&c->tir.global->type_scopes, table_init);
    TirId type = new_enum_type(c->tir, &(EnumType) {
        .scope = scope,
        .name = ctx_push_str(c, name),
        .repr = repr_type,
    });
    HashTable *table = &c->tir.global->type_scopes.ptr[scope];

    for (int32_t i = 0; i < e.member_count; i++) {
        SourceIndex member_token = get_ast_token(e.members[i], c->ast);
        String member_name = id_token_to_string(ctx_source(c), member_token);
        int32_t member_sym = c->tir.global->type_scope_symbols.len;
        int64_t prev = htable_try_insert(table, member_name, member_sym);

        if (prev >= 0) {
            SourceLoc loc = ctx_init_loc(c, member_token, member_name.len);
            print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_DEFINITION});
            AstId prev_ref = c->tir.global->type_scope_symbols.ptr[prev].ast_id;
            SourceIndex prev_token = get_ast_token(prev_ref, &c->asts[c->file]);
            SourceLoc prev_loc = ctx_init_loc(c, prev_token, member_name.len);
            print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
            add_error(c);
        }

        TirId value = new_int_constant(c->tir, type, i);
        vec_push(&c->tir.global->type_scope_symbols, (TypeScopeSymbol) {.ast_id = e.members[i], .field_index = value.id});
    }

    add_id(c, (AstRef) {node, c->file}, type);
    return type;
}

static TirId analyze_struct(Context *c, AstId node) {
    AstStruct s = get_ast_struct(node, c->ast);
    push_scope(c);

    TirId *type_param_types = arena_alloc(c->scratch, TirId, s.type_param_count);
    for (int32_t i = 0; i < s.type_param_count; i++) {
        SourceIndex token = get_ast_token(s.type_params[i], c->ast);
        String name = id_token_to_string(ctx_source(c), token);
        type_param_types[i] = new_type_parameter(c->tir, i, ctx_push_str(c, name));
        add_id(c, (AstRef) {s.type_params[i], c->file}, type_param_types[i]);
    }

    TirId *field_types = arena_alloc(c->scratch, TirId, s.field_count);
    for (int32_t i = 0; i < s.field_count; i++) {
        AstId param_type = get_ast_unary(s.fields[i], c->ast);
        field_types[i] = expect_type(c, param_type);
    }

    pop_scope(c);
    HashTable table = htable_init();
    int32_t index = 0;

    for (int32_t i = 0; i < s.field_count; i++) {
        SourceIndex field_token = get_ast_token(s.fields[i], c->ast);
        String field_name = id_token_to_string(ctx_source(c), field_token);

        int32_t field_sym = c->tir.global->type_scope_symbols.len;
        vec_push(&c->tir.global->type_scope_symbols, (TypeScopeSymbol) {.ast_id = s.fields[i], .field_index = index});
        int64_t prev = htable_try_insert(&table, field_name, field_sym);

        if (prev >= 0) {
            SourceLoc loc = ctx_init_loc(c, field_token, field_name.len);
            print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_DEFINITION});
            AstId prev_ref = c->tir.global->type_scope_symbols.ptr[prev].ast_id;
            SourceIndex prev_token = get_ast_token(prev_ref, &c->asts[c->file]);
            SourceLoc prev_loc = ctx_init_loc(c, prev_token, field_name.len);
            print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
            add_error(c);
        }

        index++;
    }

    int32_t scope = c->tir.global->type_scopes.len;
    vec_push(&c->tir.global->type_scopes, table);

    SourceIndex token = get_ast_token(node, c->ast);
    String name = id_token_to_string(ctx_source(c), token);

    if (!s.field_count) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EMPTY_STRUCT});
    }

    int32_t name_i = ctx_push_str(c, name);
    TirId inner_type = new_struct_type(c->tir, c->options->target, &(StructType) {
        .scope = scope,
        .name = name_i,
        .field_count = s.field_count,
        .fields = field_types,
    });

    inner_type = new_tagged_type(c->tir, &(TaggedType) {
        .name = name_i,
        .inner = inner_type,
        .arg_count = s.type_param_count,
        .args = type_param_types,
    });

    TirId type = inner_type;

    if (s.type_param_count) {
        type = new_generic(c->tir, inner_type, s.type_param_count, type_param_types);
    }

    add_id(c, (AstRef) {node, c->file}, type);
    vec_push(&c->tir.global->structs, inner_type);
    return type;
}

static TirId analyze_newtype(Context *c, AstId node) {
    AstNewtype n = get_ast_newtype(node, c->ast);
    push_scope(c);

    TirId *type_param_types = arena_alloc(c->scratch, TirId, n.type_param_count);
    for (int32_t i = 0; i < n.type_param_count; i++) {
        SourceIndex token = get_ast_token(n.type_params[i], c->ast);
        String name = id_token_to_string(ctx_source(c), token);
        type_param_types[i] = new_type_parameter(c->tir, i, ctx_push_str(c, name));
        add_id(c, (AstRef) {n.type_params[i], c->file}, type_param_types[i]);
    }

    TirId inner = expect_type(c, n.type);
    pop_scope(c);

    SourceIndex token = get_ast_token(node, c->ast);
    String name = id_token_to_string(ctx_source(c), token);
    TirId type = new_tagged_type(c->tir, &(TaggedType) {
        .name = ctx_push_str(c, name),
        .inner = inner,
        .arg_count = n.type_param_count,
        .args = type_param_types,
    });

    if (n.type_param_count) {
        type = new_generic(c->tir, type, n.type_param_count, type_param_types);
    }

    add_id(c, (AstRef) {node, c->file}, type);
    return type;
}

static TirId analyze_extern_function(Context *c, AstId node) {
    AstFunction f = get_ast_extern_function(node, c->ast);
    TirId *param_types = arena_alloc(c->scratch, TirId, f.param_count);
    for (int32_t i = 0; i < f.param_count; i++) {
        AstId param_type = get_ast_unary(f.params[i], c->ast);
        param_types[i] = expect_type(c, param_type);
    }

    TirId ret_type = ptype(VOID);
    if (!is_ast_null(f.ret)) {
        ret_type = expect_type(c, f.ret);
    }

    TirId type = new_function_type(c->tir, &(FunctionType) {
        .param_count = f.param_count,
        .params = param_types,
        .ret = ret_type,
    });
    SourceIndex token = get_ast_token(node, c->ast);
    String name = id_token_to_string(ctx_source(c), token);

    TirId value = new_extern_function(c->tir, type, ctx_push_str(c, name));
    add_id(c, (AstRef) {node, c->file}, value);
    vec_push(&c->tir.global->extern_functions, value);
    return value;
}

static TirId analyze_extern_mut(Context *c, AstId node) {
    AstId var_type = get_ast_unary(node, c->ast);
    TirId type = expect_type(c, var_type);
    String name = id_token_to_string(ctx_source(c), get_ast_token(node, c->ast));
    TirId value = new_extern_var(c->tir, type, ctx_push_str(c, name));
    add_id(c, (AstRef) {node, c->file}, value);
    vec_push(&c->tir.global->extern_vars, value);
    return value;
}

static TirId analyze_const(Context *c, AstId node) {
    AstId init = get_ast_unary(node, c->ast);
    TirId init_result = analyze_term(c, init, null_tir);

    add_id(c, (AstRef) {node, c->file}, init_result);

    TirTag tag = get_term_tag(c->tir, init_result);

    if (is_tir_type(tag)) {
        return init_result;
    }

    switch (get_term_tag(c->tir, init_result)) {
        case TIR_ERROR: {
            return (TirId) {0};
        }
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_EXTERN_VAR:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL:
        case TIR_STRING: {
            return init_result;
        }
        default: {
            error(c, init, &(Diagnostic) {.kind = ERROR_CONST_INIT});
            return (TirId) {0};
        }
    }
}

// Local nodes

static TirId analyze_let(Context *c, AstId node, bool mutable) {
    AstId init = get_ast_unary(node, c->ast);
    TirId init_value = expect_value(c, init, null_tir);
    TirId init_type = get_value_type(c->tir, init_value);
    if (type_is_unknown_size(c->tir, init_type)) {
        type_error(c, node, init_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    }
    int32_t var = c->local_tir->local_count++;
    TirId value = new_variable(c->tir, node, init_type, var, mutable);
    add_id(c, (AstRef) {node, c->file}, value);
    return new_instr(c->tir, mutable ? TIR_MUT : TIR_LET, node, ptype(VOID), var, init_value.id);
}

static TirId analyze_function_type(Context *c, AstId node) {
    AstCall signature = get_ast_call(node, c->ast);

    TirId *params = arena_alloc(c->scratch, TirId, signature.arg_count);
    for (int32_t i = 0; i < signature.arg_count; i++) {
        AstId param_type = get_ast_unary(signature.args[i], c->ast);
        params[i] = expect_type(c, param_type);
    }

    TirId ret = analyze_return_type(c, signature.operand);
    return new_function_type(c->tir, &(FunctionType) {
        .param_count = signature.arg_count,
        .params = params,
        .ret = ret,
    });
}

static TirId analyze_array_type(Context *c, AstId node) {
    AstBinary array = get_ast_binary(node, c->ast);
    TirId index = expect_type(c, array.left);
    TirId element = expect_type(c, array.right);

    if (get_term_tag(c->tir, index) != TIR_ARRAY_LENGTH_TYPE) {
        type_error(c, array.left, index, 0, ERROR_ARRAY_TYPE_EXPECTS_LENGTH_TYPE);
        index = null_tir;
    }

    return new_array_type(c->tir, &(ArrayType) {
        .index = index,
        .elem = element,
    });
}

static TirId analyze_array_type_sugar(Context *c, AstId node) {
    AstBinary array = get_ast_binary(node, c->ast);
    TirId length_result = expect_value_type(c, array.left, ptype(isize));
    int64_t len = 0;
    TirId index = try_get_int_const(c, length_result, &len) ? new_array_length_type(c->tir, len) : null_tir;
    TirId element = expect_type(c, array.right);
    return new_array_type(c->tir, &(ArrayType) {
        .index = index,
        .elem = element,
    });
}

static TirId analyze_id(Context *c, AstId node) {
    AstRef ref = {node, c->file};
    String name = get_id_source(c, ref);

    LocalId local = lookup_local(c, name);
    if (local.id) {
        Local info = c->locals.ptr[local.id - 1];
        if (!info.tir_ref.id) {
            return null_tir;
        }
        return info.tir_ref;
    }

    Symbol symbol = lookup(c, c->file, name);
    switch (symbol.kind) {
        case SYM_UNDEFINED: {
            diagnostic(c, ref, ERROR_UNDEFINED_NAME);

            // Check module names for hints.
            int32_t *m = htable_lookup(c->module_table, name);
            if (m && !c->module_import_notes[*m]) {
                SourceLoc loc = get_ast_location(c, (AstRef) {ref.node, ref.file});
                print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_FORGOT_IMPORT});
                c->module_import_notes[*m] = true;
            }

            add_id(c, ref, null_tir);
            break;
        }
        case SYM_BUILTIN: {
            return (TirId) {symbol.builtin};
        }
        case SYM_GLOBAL: {
            return resolve_global(c, ref, symbol.global);
        }
        case SYM_LOCAL: {
            break;
        }
    }

    return null_tir;
}

static TirId analyze_int(Context *c, AstId node, TirId hint) {
    int64_t i = get_ast_int(node, c->ast);
    TirId type = int_fits_in_type(i, hint, c->options->target) ? hint : ptype(i64);
    return new_int_constant(c->tir, type, i);
}

static TirId analyze_float(Context *c, AstId node, TirId hint) {
    double f = get_ast_float(node, c->ast);
    TirId type = type_is_float(hint) ? hint : ptype(f64);
    return new_float_constant(c->tir, type, f);
}

static TirId analyze_char(Context *c, AstId node) {
    int64_t i = get_ast_int(node, c->ast);
    return new_int_constant(c->tir, ptype(char), i);
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

static TirId analyze_string(Context *c, AstId node) {
    int64_t len = string_token_byte_length(ctx_source(c), get_ast_token(node, c->ast));
    int32_t index;
    char *buffer;

    if (c->tir.thread) {
        index = c->tir.thread->strtab.len;
        buffer = vec_grow(&c->tir.thread->strtab, len + 4);
    } else {
        index = c->tir.global->strtab.len;
        buffer = vec_grow(&c->tir.global->strtab, len + 4);
    }

    buffer[0] = (unsigned char) (len & 0xFF);
    buffer[1] = (unsigned char) ((len >> 8) & 0xFF);
    buffer[2] = (unsigned char) ((len >> 16) & 0xFF);
    buffer[3] = (unsigned char) ((len >> 24) & 0xFF);
    int32_t token = get_ast_token(node, c->ast).index;

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

    TirId type = new_array_type(c->tir, &(ArrayType) {
        .index = new_array_length_type(c->tir, len),
        .elem = ptype(char),
    });
    return new_string_constant(c->tir, type, index);
}

static TirId analyze_bool(Context *c, AstId node) {
    int64_t i = get_ast_int(node, c->ast);
    return new_int_constant(c->tir, ptype(bool), i);
}

static TirId analyze_null(Context *c, TirId hint) {
    TirId type = hint;
    if (!remove_pointer(c->tir, hint).id) {
        type = new_mut_ptr_type(c->tir, ptype(byte));
    }
    return new_null_constant(c->tir, type);
}

static TirId analyze_un_arithmetic(Context *c, AstId node, TirId hint, TirTag tag) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_value = expect_value(c, operand, hint);
    TirId operand_type = get_value_type(c->tir, operand_value);

    if (!type_is_arithmetic(operand_type)) {
        type_error(c, operand, operand_type, 0, ERROR_UNARY_UNEXPECTED_OPERAND);
        return null_tir;
    }

    return new_unary_tir(c->tir, tag, node, operand_type, operand_value);
}

static TirId analyze_not(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_value = expect_value_type(c, operand, ptype(bool));
    return new_unary_tir(c->tir, TIR_NOT, node, ptype(bool), operand_value);
}

static TirId analyze_address(Context *c, AstId node, TirId hint) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_value = expect_value(c, operand, remove_any_pointer(c->tir, hint));
    TirId operand_type = get_value_type(c->tir, operand_value);
    switch (get_value_category(c->tir, operand_value)) {
        case VALUE_INVALID: {
            break;
        }
        case VALUE_TEMPORARY: {
            TirId type = new_mut_ptr_type(c->tir, operand_type);
            return new_unary_tir(c->tir, TIR_ADDRESS_OF_TEMPORARY, node, type, operand_value);
        }
        case VALUE_PLACE: {
            TirId type = new_ptr_type(c->tir, operand_type);
            return new_unary_tir(c->tir, TIR_ADDRESS, node, type, operand_value);
        }
        case VALUE_MUTABLE_PLACE: {
            TirId type = new_mut_ptr_type(c->tir, operand_type);
            return new_unary_tir(c->tir, TIR_ADDRESS, node, type, operand_value);
        }
        case VALUE_MULTIVALUE: {
            return expect_value(c, operand, hint);
        }
    }
    return null_tir;
}

static TirId analyze_deref(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);

    TirId operand_value = analyze_term(c, operand, null_tir);

    if (is_tir_type(get_term_tag(c->tir, operand_value))) {
        return new_ptr_type(c->tir, operand_value);
    }

    TirId operand_type = get_value_type(c->tir, operand_value);

    TirId type = remove_pointer(c->tir, operand_type);
    if (!type.id) {
        type_error(c, operand, operand_type, 0, ERROR_DEREF_UNEXPECTED_OPERAND);
        return null_tir;
    }

    return new_unary_tir(c->tir, TIR_DEREF, node, type, operand_value);
}

static TirId analyze_mut_ptr_type(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_type = expect_type(c, operand);
    return new_mut_ptr_type(c->tir, operand_type);
}

static TirId analyze_slice_type(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_type = expect_type(c, operand);
    return new_slice_type(c->tir, operand_type);
}

static TirId analyze_mut_slice_type(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId operand_type = expect_type(c, operand);
    return new_mut_slice_type(c->tir, operand_type);
}

static AstId get_call_arg(AstCall *call, int32_t i) {
    return i < call->arg_count ? call->args[i] : null_ast;
}

static bool expect_arg_count(Context *c, AstId node, int32_t param_count) {
    AstCall call = get_ast_call(node, c->ast);
    if (call.arg_count == param_count) {
        return true;
    }
    error(c, call.operand, &(Diagnostic) {.kind = ERROR_WRONG_COUNT, .count_error = {param_count, call.arg_count}});
    return false;
}

static TirId get_internal_term(Context *c, PrimitiveTerm p) {
    return c->tir_refs[p - TERM_COUNT];
}

static TirId analyze_alignof(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_type = expect_type(c, get_call_arg(&call, 0));
    int32_t i = alignof_type(c->tir, operand_type, c->options->target);
    expect_arg_count(c, node, 1);
    if (i >= 1) {
        TirId type_alignment_tag = get_internal_term(c, BUILTIN_ALIGNMENT);
        type_alignment_tag = get_generic_term(c->tir, type_alignment_tag).inner;
        TirId type = replace_type_parameters(type_alignment_tag, &(ReplaceTypeInfo) {
            .c = c->tir,
            .args = &operand_type,
            .scratch = *c->scratch,
            .target = c->options->target,
        });
        return new_int_constant(c->tir, type, i);
    }
    type_error(c, node, operand_type, 0, ERROR_TYPE_UNKNOWN_TYPE_ALIGNMENT);
    return null_tir;
}

static TirId analyze_sizeof(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_type = expect_type(c, get_call_arg(&call, 0));
    int64_t i = sizeof_type(c->tir, operand_type, c->options->target);
    expect_arg_count(c, node, 1);
    if (i >= 1) {
        TirId type_size_tag = get_internal_term(c, BUILTIN_SIZE);
        type_size_tag = get_generic_term(c->tir, type_size_tag).inner;
        TirId type = replace_type_parameters(type_size_tag, &(ReplaceTypeInfo) {
            .c = c->tir,
            .args = &operand_type,
            .scratch = *c->scratch,
            .target = c->options->target,
        });
        return new_int_constant(c->tir, type, i);
    }
    type_error(c, node, operand_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    return null_tir;
}

static TirTag get_cast_type(Context *c, TirId operand_type, TirId cast_type) {
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

static TirId analyze_cast(Context *c, AstId node, TirId cast_type) {
    AstCall call = get_ast_call(node, c->ast);

    if (!cast_type.id) {
        error(c, node, &(Diagnostic) {.kind = ERROR_TYPE_INFERENCE});
        return null_tir;
    }

    TirId operand_value = expect_value(c, get_call_arg(&call, 0), cast_type);
    TirId operand_type = get_value_type(c->tir, operand_value);
    expect_arg_count(c, node, 1);

    if (operand_type.id == cast_type.id) {
        return operand_value;
    }

    TirTag cast_kind = get_cast_type(c, operand_type, cast_type);
    if ((int) cast_kind == -1) {
        double_type_error(c, get_call_arg(&call, 0), operand_type, cast_type, ERROR_CAST);
        return null_tir;
    }

    return new_unary_tir(c->tir, cast_kind, node, cast_type, operand_value);
}

static TirId analyze_zero_extend(Context *c, AstId node, TirId hint) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_value = expect_value(c, get_call_arg(&call, 0), hint);
    expect_arg_count(c, node, 1);

    if (!hint.id || !type_is_fixed_int(hint)) {
        error(c, node, &(Diagnostic) {.kind = ERROR_TYPE_INFERENCE});
        return null_tir;
    }

    TirId operand_type = get_value_type(c->tir, operand_value);
    if (!type_is_fixed_int(operand_type)) {
        double_type_error(c, call.operand, operand_type, hint, ERROR_CAST);
        return null_tir;
    }

    if (bigger_primitive_type(hint, operand_type, c->options->target).id == operand_type.id) {
        return operand_value;
    }

    return new_unary_tir(c->tir, TIR_ZEXT, node, hint, operand_value);
}

static TirId analyze_slice_constructor(Context *c, AstId node, TirId hint) {
    AstCall call = get_ast_call(node, c->ast);
    TirId length_result = expect_value_type(c, get_call_arg(&call, 0), ptype(isize));
    TirId data_value = expect_value(c, get_call_arg(&call, 1), replace_slice_with_pointer(c->tir, hint));
    expect_arg_count(c, node, 2);
    TirId data_type = get_value_type(c->tir, data_value);
    TirId type = replace_pointer_with_slice(c->tir, data_type);

    if (!type.id && call.arg_count >= 2) {
        type_error(c, get_call_arg(&call, 1), data_type, 0, ERROR_SLICE_CTOR_EXPECTS_POINTER);
        return null_tir;
    }

    int32_t extra[2] = {
        length_result.id,
        data_value.id,
    };
    return new_instr(c->tir, TIR_NEW_STRUCT, node, type, push_extra(c, extra, ArrayLength(extra)), 2);
}

static TirId analyze_linear(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_type = expect_type(c, get_call_arg(&call, 0));
    expect_arg_count(c, node, 1);
    return new_linear_type(c->tir, operand_type);
}

static TirId analyze_array_length_type(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_value = expect_value_type(c, get_call_arg(&call, 0), ptype(isize));
    expect_arg_count(c, node, 1);

    int64_t i = 0;
    if (try_get_int_const(c, operand_value, &i)) {
        return new_array_length_type(c->tir, 1);
    }

    return null_tir;
}

static TirId analyze_bin_arithmetic(Context *c, AstId node, TirId hint, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_value(c, bin.left, hint);
    TirId left_type = get_value_type(c->tir, left_value);
    left_type = remove_tags(c->tir, left_type);
    TirId right_value = expect_value(c, bin.right, left_type);
    TirId right_type = get_value_type(c->tir, right_value);
    right_type = remove_tags(c->tir, right_type);

    if (left_type.id != right_type.id || !type_is_arithmetic(left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_tir;
    }

    return new_binary_tir(c->tir, tag, node, left_type, left_value, right_value);
}

static TirId analyze_bin_bit(Context *c, AstId node, TirId hint, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_value(c, bin.left, hint);
    TirId left_type = get_value_type(c->tir, left_value);
    left_type = remove_tags(c->tir, left_type);
    TirId right_value = expect_value(c, bin.right, left_type);
    TirId right_type = get_value_type(c->tir, right_value);
    right_type = remove_tags(c->tir, right_type);

    if (left_type.id != right_type.id || !type_is_int(left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_tir;
    }

    return new_binary_tir(c->tir, tag, node, left_type, left_value, right_value);
}

static TirId analyze_eq(Context *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_value(c, bin.left, null_tir);
    TirId left_type = get_value_type(c->tir, left_value);
    TirId right_value = expect_value(c, bin.right, left_type);
    TirId right_type = get_value_type(c->tir, right_value);

    if (left_type.id != right_type.id || !is_equality_type(c->tir, left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_tir;
    }

    return new_binary_tir(c->tir, tag, node, ptype(bool), left_value, right_value);
}

static TirId analyze_rel(Context *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_value(c, bin.left, null_tir);
    TirId left_type = get_value_type(c->tir, left_value);
    TirId right_value = expect_value(c, bin.right, left_type);
    TirId right_type = get_value_type(c->tir, right_value);

    if (left_type.id != right_type.id || !is_relative_type(c->tir, left_type)) {
        double_type_error(c, node, left_type, right_type, ERROR_BINARY_UNEXPECTED_OPERANDS);
        return null_tir;
    }

    return new_binary_tir(c->tir, tag, node, ptype(bool), left_value, right_value);
}

static TirId analyze_logic(Context *c, AstId node, bool is_and) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_value_type(c, bin.left, ptype(bool));
    TirId right_value = expect_value_type(c, bin.right, ptype(bool));

    TirId true_value = new_int_constant(c->tir, ptype(bool), 1);
    TirId false_value = new_int_constant(c->tir, ptype(bool), 0);

    int32_t branches_tir[4] = {
        is_and ? false_value.id : true_value.id, is_and ? false_value.id : true_value.id,
        0, right_value.id,
    };
    int32_t extra[] = {
        push_extra(c, branches_tir, 4),
        2,
    };
    return new_instr(c->tir, TIR_SWITCH, node, ptype(bool), left_value.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_assign(Context *c, AstId node) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_mutable_place(c, bin.left, null_tir);
    TirId left_type = get_value_type(c->tir, left_value);
    TirId right_value = expect_value_type(c, bin.right, left_type);
    if (type_is_unknown_size(c->tir, left_type)) {
        type_error(c, node, left_type, 0, ERROR_TYPE_UNKNOWN_TYPE_SIZE);
    }
    return new_binary_tir(c->tir, TIR_ASSIGN, node, ptype(VOID), left_value, right_value);
}

static TirId analyze_assign_arithmetic(Context *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_mutable_place(c, bin.left, null_tir);
    TirId left_type = get_value_type(c->tir, left_value);
    TirId right_value = expect_value_type(c, bin.right, left_type);
    if (!type_is_arithmetic(left_type)) {
        double_type_error(c, bin.left, left_type, get_value_type(c->tir, right_value), ERROR_BINARY_UNEXPECTED_OPERANDS);
    }
    return new_binary_tir(c->tir, tag, node, ptype(VOID), left_value, right_value);
}

static TirId analyze_assign_bit(Context *c, AstId node, TirTag tag) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId left_value = expect_mutable_place(c, bin.left, null_tir);
    TirId left_type = get_value_type(c->tir, left_value);
    TirId right_value = expect_value_type(c, bin.right, left_type);
    if (!type_is_int(left_type)) {
        double_type_error(c, bin.left, left_type, get_value_type(c->tir, right_value), ERROR_BINARY_UNEXPECTED_OPERANDS);
    }
    return new_binary_tir(c->tir, tag, node, ptype(VOID), left_value, right_value);
}

static TirId implicit_pointer_deref(Context *c, AstId node, TirId value) {
    TirId type = get_value_type(c->tir, value);
    TirId inner_type = remove_pointer(c->tir, type);
    if (!inner_type.id) {
        return value;
    }
    return new_unary_tir(c->tir, TIR_DEREF, node, inner_type, value);
}

static int32_t find_field(Context *c, TirId type, String name) {
    int32_t scope = get_struct_type(c->tir, type).scope;
    int32_t *sym = htable_lookup(&c->tir.global->type_scopes.ptr[scope], name);

    if (!sym) {
        return -1;
    }

    return *sym;
}

static TirId resolve_enum_member(Context *c, AstId node, TirId type) {
    SourceIndex field_token = get_ast_token(node, c->ast);
    String field_name = id_token_to_string(ctx_source(c), field_token);
    int32_t scope = get_enum_type(c->tir, type).scope;
    int32_t *sym_ptr = htable_lookup(&c->tir.global->type_scopes.ptr[scope], field_name);

    if (!sym_ptr) {
        type_error(c, node, type, 0, ERROR_UNDEFINED_TYPE_SCOPE);
        return null_tir;
    }

    TypeScopeSymbol sym = c->tir.global->type_scope_symbols.ptr[*sym_ptr];
    return (TirId) {sym.field_index};
}

static TirId analyze_enum_member(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    TirId type = expect_type(c, operand);

    if (get_term_tag(c->tir, type) != TIR_ENUM_TYPE) {
        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_SCOPE);
        return null_tir;
    }

    return resolve_enum_member(c, node, type);
}

static TirId analyze_enum_member_inferred(Context *c, AstId node, TirId hint) {
    if (!hint.id || get_term_tag(c->tir, hint) != TIR_ENUM_TYPE) {
        error(c, node, &(Diagnostic) {.kind = ERROR_TYPE_INFERENCE});
        return null_tir;
    }

    return resolve_enum_member(c, node, hint);
}

static TirId resolve_length(Context *c, AstId node, TirId array_like) {
    TirId type = get_value_type(c->tir, array_like);
    switch (get_term_tag(c->tir, type)) {
        case TIR_ARRAY_TYPE: {
            TirId index_type = get_array_type(c->tir, type).index;
            return new_int_constant(c->tir, ptype(isize), get_array_length_type(c->tir, index_type));
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return new_instr(c->tir, TIR_ACCESS, node, ptype(isize), array_like.id, 0);
        }
        default: {
            return null_tir;
        }
    }
}

static TirId resolve_slice_data(Context *c, AstId node, TirId array_like) {
    TirId type = get_value_type(c->tir, array_like);
    type = replace_slice_with_pointer(c->tir, type);
    if (!type.id) {
        return null_tir;
    }
    return new_instr(c->tir, TIR_ACCESS, node, type, array_like.id, 1);
}

static TirId analyze_access(Context *c, AstId node) {
    AstId operand = get_ast_unary(node, c->ast);
    SourceIndex field_token = get_ast_token(node, c->ast);
    String field_name = id_token_to_string(ctx_source(c), field_token);
    TirId operand_value = analyze_term(c, operand, null_tir);

    if (operand_value.id >= BUILTIN_TERM_END && operand_value.id < 0) {
        int32_t module = ~operand_value.id;
        int32_t *def_ptr = htable_lookup(&c->modules[module].public_scope, field_name);

        if (!def_ptr) {
            diagnostic(c, (AstRef) {operand, c->file}, ERROR_UNDEFINED_NAME_FROM_MODULE);

            // Check private namespace for hints.
            def_ptr = htable_lookup(&c->modules[module].private_scope, field_name);
            if (def_ptr) {
                AstRef ast_ref = c->ast_refs[*def_ptr];
                SourceLoc loc = get_ast_location(c, ast_ref);
                print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_PRIVATE_DEFINITION});
            }

            return null_tir;
        }

        DefId global = {*def_ptr};
        return resolve_global(c, (AstRef) {operand, c->file}, global);
    }

    TirTag tag = get_term_tag(c->tir, operand_value);

    if (is_tir_type(tag)) {
        return analyze_enum_member(c, node);
    }

    if (!is_tir_value(tag)) {
        diagnostic(c, (AstRef) {node, c->file}, ERROR_ACCESS_OPERAND_ROLE);
        return null_tir;
    }

    operand_value = implicit_pointer_deref(c, node, operand_value);
    TirId operand_type = get_value_type(c->tir, operand_value);
    TirId type = remove_tags(c->tir, operand_type);
    TirId slice_elem_type = remove_slice(c->tir, type);

    if (slice_elem_type.id) {
        if (equals(field_name, (String) Str("length"))) {
            return resolve_length(c, node, operand_value);
        }

        if (equals(field_name, (String) Str("data"))) {
            return resolve_slice_data(c, node, operand_value);
        }

        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_tir;
    }

    TirId array_elem_type = remove_array_like(c->tir, type);

    if (array_elem_type.id) {
        if (equals(field_name, (String) Str("length"))) {
            return resolve_length(c, node, operand_value);
        }

        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_tir;
    }

    if (get_term_tag(c->tir, type) != TIR_STRUCT_TYPE) {
        type_error(c, operand, operand_type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_tir;
    }

    int32_t field_sym = find_field(c, type, field_name);

    if (field_sym == -1) {
        type_error(c, operand, type, 0, ERROR_UNDEFINED_TYPE_FIELD);
        return null_tir;
    }

    TypeScopeSymbol sym = c->tir.global->type_scope_symbols.ptr[field_sym];
    TirId result_type = get_struct_type_field(c->tir, type, sym.field_index);
    return new_instr(c->tir, TIR_ACCESS, node, result_type, operand_value.id, sym.field_index);
}

static TirId analyze_type_hint(Context *c, AstId node) {
    AstBinary bin = get_ast_binary(node, c->ast);
    TirId cast_type = expect_type(c, bin.right);
    return expect_value_type(c, bin.left, cast_type);
}

static TirId analyze_struct_ctor(Context *c, AstId node, GenericTerm *term) {
    AstCall call = get_ast_call(node, c->ast);
    TirId *args_tir = arena_alloc(c->scratch, TirId, call.arg_count);
    TirId *type_args = arena_alloc(c->scratch, TirId, term->type_count);
    TirId inner = remove_tags(c->tir, term->inner);
    bool type_args_inferred = true;

    for (int32_t i = 0; i < call.arg_count; i++) {
        TirId field_type = get_struct_type_field(c->tir, inner, i);
        if (term->type_count) {
            TirId arg_result = expect_value(c, call.args[i], field_type);
            TirId arg_type = get_value_type(c->tir, arg_result);
            if (!match_type_parameters(c->tir, type_args, field_type, arg_type)) {
                type_args_inferred = false;
            }
            args_tir[i] = arg_result;
        } else {
            args_tir[i] = expect_value_type(c, call.args[i], field_type);
        }
    }

    if (type_args_inferred && term->type_count) {
        for (int32_t i = 0; i < call.arg_count; i++) {
            TirId field_type = get_struct_type_field(c->tir, inner, i);
            field_type = replace_type_parameters(field_type, &(ReplaceTypeInfo) {
                .c = c->tir,
                .args = type_args,
                .scratch = *c->scratch,
                .target = c->options->target,
            });
            args_tir[i] = apply_implicit_conversion(c, call.args[i], args_tir[i], field_type);
        }
    }

    int32_t field_count = get_struct_type(c->tir, inner).field_count;
    if (field_count != call.arg_count) {
        type_error(c, call.operand, term->inner, call.arg_count, ERROR_FIELD_COUNT);
    }

    if (!type_args_inferred) {
        type_error(c, call.operand, term->inner, call.arg_count, ERROR_TYPE_ARGUMENT_INFERENCE);
        return null_tir;
    }

    TirId type = term->inner;
    if (term->type_count) {
        type = replace_type_parameters(term->inner, &(ReplaceTypeInfo) {
            .c = c->tir,
            .args = type_args,
            .scratch = *c->scratch,
            .target = c->options->target,
        });
    }
    return new_instr(c->tir, TIR_NEW_STRUCT, node, type, push_extra(c, (int32_t *) args_tir, call.arg_count), call.arg_count);
}

static TirId analyze_linear_ctor(Context *c, AstId node, TirId linear_type) {
    AstCall call = get_ast_call(node, c->ast);

    if (1 != call.arg_count) {
        type_error(c, call.operand, linear_type, 0, ERROR_LINEAR_CTOR_COUNT);
        return null_tir;
    }

    TirId param_type = get_linear_elem_type(c->tir, linear_type);
    TirId arg_result = expect_value_type(c, get_call_arg(&call, 0), param_type);
    return new_unary_tir(c->tir, TIR_NOP, node, linear_type, arg_result);
}

static TirId analyze_constructor(Context *c, AstId node, GenericTerm *term) {
    AstCall call = get_ast_call(node, c->ast);
    TirId inner = remove_tags(c->tir, term->inner);
    switch (get_term_tag(c->tir, inner)) {
        case TIR_STRUCT_TYPE: return analyze_struct_ctor(c, node, term);
        case TIR_LINEAR_TYPE: return analyze_linear_ctor(c, node, term->inner);
        default: type_error(c, call.operand, term->inner, 0, ERROR_TYPE_CONSTRUCTOR_TYPE); return null_tir;
    }
}

static TirId analyze_function_call(Context *c, AstId node, GenericTerm *term) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_type = get_value_type(c->tir, term->inner);

    if (get_term_tag(c->tir, operand_type) != TIR_FUNCTION_TYPE) {
        type_error(c, call.operand, operand_type, 0, ERROR_CALLEE);
        return null_tir;
    }

    FunctionType func_type = get_function_type(c->tir, operand_type);
    TirId *args_tir = arena_alloc(c->scratch, TirId, call.arg_count);
    TirId *type_args = arena_alloc(c->scratch, TirId, term->type_count);
    bool type_args_inferred = true;

    for (int32_t i = 0; i < call.arg_count; i++) {
        TirId param_type = get_function_type_param(c->tir, operand_type, i);
        if (term->type_count) {
            TirId arg_result = expect_value(c, call.args[i], param_type);
            TirId arg_type = get_value_type(c->tir, arg_result);
            if (!match_type_parameters(c->tir, type_args, param_type, arg_type)) {
                type_args_inferred = false;
            }
            args_tir[i] = arg_result;
        } else {
            args_tir[i] = expect_value_type(c, call.args[i], param_type);
        }
    }

    if (type_args_inferred && term->type_count) {
        for (int32_t i = 0; i < call.arg_count; i++) {
            TirId param_type = get_function_type_param(c->tir, operand_type, i);
            param_type = replace_type_parameters(param_type, &(ReplaceTypeInfo) {
                .c = c->tir,
                .args = type_args,
                .scratch = *c->scratch,
                .target = c->options->target,
            });
            args_tir[i] = apply_implicit_conversion(c, call.args[i], args_tir[i], param_type);
        }
    }

    if (func_type.param_count != call.arg_count) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_ARGUMENT_COUNT);
        return null_tir;
    }

    if (!type_args_inferred) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_TYPE_ARGUMENT_INFERENCE);
        return null_tir;
    }

    TirId result_type = func_type.ret;
    if (term->type_count) {
        result_type = replace_type_parameters(func_type.ret, &(ReplaceTypeInfo) {
            .c = c->tir,
            .args = type_args,
            .scratch = *c->scratch,
            .target = c->options->target,
        });
    }

    return new_instr(
        c->tir,
        TIR_CALL,
        node,
        result_type,
        term->inner.id,
        push_extra(c, (int32_t *) args_tir, call.arg_count)
    );
}

static TirId analyze_call(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_value = analyze_term(c, call.operand, null_tir);
    GenericTerm g = get_generic_term(c->tir, operand_value);

    if (is_tir_type(get_term_tag(c->tir, g.inner))) {
        return analyze_constructor(c, node, &g);
    }

    if (is_tir_value(get_term_tag(c->tir, g.inner))) {
        return analyze_function_call(c, node, &g);
    }

    diagnostic(c, (AstRef) {node, c->file}, ERROR_CALL_OPERAND_ROLE);
    return null_tir;
}

static TirId analyze_tagged_type(Context *c, AstId node, TirId term) {
    AstCall call = get_ast_call(node, c->ast);
    TirId *arg_types = arena_alloc(c->scratch, TirId, call.arg_count);
    for (int32_t i = 0; i < call.arg_count; i++) {
        arg_types[i] = expect_type(c, call.args[i]);
    }
    if (!term.id) {
        return null_tir;
    }
    GenericTerm g = get_generic_term(c->tir, term);
    if (g.type_count != call.arg_count) {
        type_error(c, call.operand, g.inner, call.arg_count, ERROR_ARGUMENT_COUNT);
        return null_tir;
    }
    return replace_type_parameters(g.inner, &(ReplaceTypeInfo) {
        .c = c->tir,
        .args = arg_types,
        .scratch = *c->scratch,
        .target = c->options->target,
    });
}

static TirId analyze_index(Context *c, AstId node, TirId hint) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_value = analyze_term(c, call.operand, null_tir);

    switch ((PrimitiveTerm) operand_value.id) {
        case BUILTIN_ALIGNOF: return analyze_alignof(c, node);
        case BUILTIN_SIZEOF: return analyze_sizeof(c, node);
        case BUILTIN_CAST: return analyze_cast(c, node, hint);
        case BUILTIN_ZERO_EXTEND: return analyze_zero_extend(c, node, hint);
        case BUILTIN_SLICE: return analyze_slice_constructor(c, node, hint);
        case BUILTIN_AFFINE: return analyze_linear(c, node);
        case BUILTIN_ARRAY_LENGTH_TYPE: return analyze_array_length_type(c, node);
        default: break;
    }

    if (get_term_tag(c->tir, operand_value) == TIR_GENERIC) {
        return analyze_tagged_type(c, node, operand_value);
    }

    if (!is_tir_value(get_term_tag(c->tir, operand_value))) {
        diagnostic(c, (AstRef) {node, c->file}, ERROR_INDEX_OPERAND_ROLE);
        return null_tir;
    }

    operand_value = implicit_pointer_deref(c, node, operand_value);
    TirId operand_type = get_value_type(c->tir, operand_value);
    bool error = false;

    if (!remove_array_like(c->tir, operand_type).id) {
        type_error(c, call.operand, operand_type, 0, ERROR_INDEX_OPERAND);
        error = true;
    }

    if (call.arg_count != 1) {
        type_error(c, call.operand, operand_type, call.arg_count, ERROR_INDEX_COUNT);
        return null_tir;
    }

    TirId arg_result = expect_value_type(c, call.args[0], ptype(isize));

    TirId elem_type = remove_c_pointer_like(c->tir, operand_type);
    if (type_is_unknown_size(c->tir, elem_type)) {
        type_error(c, call.operand, elem_type, 0, ERROR_INDEX_UNKNOWN_TYPE_SIZE);
    }

    if (error) {
        return null_tir;
    }

    return new_binary_tir(c->tir, TIR_INDEX, node, elem_type, operand_value, arg_result);
}

static TirId analyze_slice(Context *c, AstId node) {
    AstCall call = get_ast_call(node, c->ast);
    TirId operand_value = expect_value(c, call.operand, null_tir);
    TirId operand_type = get_value_type(c->tir, operand_value);
    TirId elem_type = remove_array_like(c->tir, operand_type);

    if (!elem_type.id) {
        type_error(c, call.operand, elem_type, 0, ERROR_INDEX_OPERAND);
        return null_tir;
    }

    TirId type;
    if (remove_slice(c->tir, operand_type).id) {
        type = operand_type;
    } else if (get_term_tag(c->tir, operand_type) == TIR_ARRAY_TYPE
        && get_value_category(c->tir, operand_value) == VALUE_MUTABLE_PLACE)
    {
        type = new_mut_slice_type(c->tir, elem_type);
    } else {
        type = new_slice_type(c->tir, elem_type);
    }

    TirId low_result;
    if (!is_ast_null(get_call_arg(&call, 0))) {
        low_result = expect_value_type(c, get_call_arg(&call, 0), ptype(isize));
    } else {
        low_result = new_int_constant(c->tir, ptype(isize), 0);
    }

    TirId high_result;
    if (!is_ast_null(get_call_arg(&call, 1))) {
        high_result = expect_value_type(c, get_call_arg(&call, 1), ptype(isize));
    } else {
        high_result = resolve_length(c, node, operand_value);
    }

    int32_t extra[] = {
        low_result.id,
        high_result.id,
    };
    return new_instr(
        c->tir,
        TIR_SLICE,
        node,
        type,
        operand_value.id,
        push_extra(c, extra, ArrayLength(extra))
    );
}

static TirId analyze_list(Context *c, AstId node, TirId hint) {
    AstList list = get_ast_list(node, c->ast);
    TirId elem_type = remove_c_pointer_like(c->tir, hint);

    int32_t *args_tir = arena_alloc(c->scratch, int32_t, list.count);
    int32_t index = 0;

    for (int32_t i = 0; i < list.count; i++) {
        if (elem_type.id) {
            TirId arg_result = expect_value_type(c, list.nodes[i], elem_type);
            args_tir[index++] = arg_result.id;
        } else {
            TirId arg_result = expect_value(c, list.nodes[i], null_tir);
            elem_type = get_value_type(c->tir, arg_result);
            args_tir[index++] = arg_result.id;
        }
    }

    if (!list.count) {
        error(c, node, &(Diagnostic) {.kind = ERROR_EMPTY_ARRAY});
        return null_tir;
    }

    TirId type = new_array_type(c->tir, &(ArrayType) {
        .index = new_array_length_type(c->tir, list.count),
        .elem = elem_type,
    });
    return new_instr(c->tir, TIR_NEW_ARRAY, node, type, push_extra(c, args_tir, list.count), list.count);
}

static TirId analyze_if(Context *c, AstId node) {
    AstIf if_ = get_ast_if(node, c->ast);
    TirId cond_result = expect_value_type(c, if_.condition, ptype(bool));
    push_scope(c);
    TirBlock true_tir = analyze_block(c, if_.true_block, null_tir);
    pop_scope(c);
    TirBlock false_tir = {0};
    if (!is_ast_null(if_.false_block)) {
        push_scope(c);
        false_tir = analyze_block(c, if_.false_block, null_tir);
        pop_scope(c);
    }
    int32_t extra[] = {
        true_tir.index,
        true_tir.length,
        false_tir.index,
        false_tir.length,
    };
    return new_instr(c->tir, TIR_IF, node, ptype(VOID), cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_while(Context *c, AstId node) {
    AstBinary while_ = get_ast_binary(node, c->ast);
    c->loop_depth++;
    TirId cond_result = expect_value_type(c, while_.left, ptype(bool));
    push_scope(c);
    TirBlock block_tir = analyze_block(c, while_.right, null_tir);
    pop_scope(c);
    c->loop_depth--;
    int32_t extra[] = {
        0,
        block_tir.index,
        block_tir.length,
    };
    return new_instr(c->tir, TIR_LOOP, node, ptype(VOID), cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_for_helper(Context *c, AstId node) {
    AstFor for_ = get_ast_for(get_ast_unary(node, c->ast), c->ast);
    push_scope(c);
    return analyze_term(c, for_.init, null_tir);
}

static TirId analyze_for(Context *c, AstId node) {
    AstFor for_ = get_ast_for(node, c->ast);
    c->loop_depth++;
    TirId cond_result = expect_value_type(c, for_.condition, ptype(bool));
    TirBlock block_tir = analyze_block(c, for_.block, null_tir);
    TirId next_tir = expect_value(c, for_.next, null_tir);
    c->loop_depth--;
    int32_t extra[] = {
        next_tir.id,
        block_tir.index,
        block_tir.length,
    };
    pop_scope(c);
    return new_instr(c->tir, TIR_LOOP, node, ptype(VOID), cond_result.id, push_extra(c, extra, ArrayLength(extra)));
}

static void validate_exhaustive_enum_switch(Context *c, AstId node, EnumType *type, int32_t *branches) {
    AstCall switch_ = get_ast_call(node, c->ast);
    HashTable const *scope = &c->tir.global->type_scopes.ptr[type->scope];
    bool *seen_enum_values = arena_alloc(c->scratch, bool, scope->count);
    AstId else_case = null_ast;

    for (int32_t i = 0; i < switch_.arg_count; i++) {
        AstBinary branch = get_ast_binary(switch_.args[i], c->ast);

        if (is_ast_null(branch.left)) {
            else_case = switch_.args[i];
            continue;
        }

        int64_t value = 0;
        try_get_int_const(c, (TirId) {branches[i * 2]}, &value);
        if (seen_enum_values[value]) {
            error(c, branch.left, &(Diagnostic) {.kind = ERROR_DUPLICATE_SWITCH_CASE});
        } else {
            seen_enum_values[value] = true;
        }
    }

    bool has_all = true;
    for (int32_t i = 0; i < scope->count; i++) {
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

static TirId analyze_switch(Context *c, AstId node, TirId hint) {
    AstCall switch_ = get_ast_call(node, c->ast);
    TirId pattern_type = ptype(bool);
    TirId cond_value = null_tir;

    if (!is_ast_null(switch_.operand)) {
        TirId cond_result = expect_value(c, switch_.operand, null_tir);
        pattern_type = get_value_type(c->tir, cond_result);
        cond_value = cond_result;
    }

    int32_t *branches_tir = arena_alloc(c->scratch, int32_t, switch_.arg_count * 2);
    TirId result_type = hint;
    bool consistent_types = true;
    AstId first_incompatible_case = node;
    AstId else_case = null_ast;

    for (int32_t i = 0; i < switch_.arg_count; i++) {
        AstBinary branch = get_ast_binary(switch_.args[i], c->ast);

        if (!is_ast_null(branch.left)) {
            TirId pattern_result = expect_value_type(c, branch.left, pattern_type);
            branches_tir[i * 2] = pattern_result.id;
        } else {
            branches_tir[i * 2] = 0;
            else_case = switch_.args[i];
        }

        TirId value_result = expect_value_type(c, branch.right, hint);
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
                if (get_term_tag(c->tir, pattern_type) == TIR_ENUM_TYPE) {
                    EnumType enum_type = get_enum_type(c->tir, pattern_type);
                    validate_exhaustive_enum_switch(c, node, &enum_type, branches_tir);
                } else if (is_ast_null(else_case)) {
                    error(c, node, &(Diagnostic) {.kind = ERROR_SWITCH_NOT_EXHAUSTIVE});
                }
            }
        }
    }

    int32_t extra[] = {
        push_extra(c, branches_tir, switch_.arg_count * 2),
        switch_.arg_count,
    };
    return new_instr(c->tir, TIR_SWITCH, node, result_type, cond_value.id, push_extra(c, extra, ArrayLength(extra)));
}

static TirId analyze_break(Context *c, AstId node) {
    if (!c->loop_depth) {
        error(c, node, &(Diagnostic) {.kind = ERROR_MISPLACED_BREAK});
    }

    return new_instr(c->tir, TIR_BREAK, node, ptype(VOID), 0, 0);
}

static TirId analyze_continue(Context *c, AstId node) {
    if (!c->loop_depth) {
        error(c, node, &(Diagnostic) {.kind = ERROR_MISPLACED_CONTINUE});
    }

    return new_instr(c->tir, TIR_CONTINUE, node, ptype(VOID), 0, 0);
}

static TirId analyze_term(Context *c, AstId node, TirId hint) {
    switch (get_ast_tag(node, c->ast)) {
        case AST_ARRAY_TYPE: return analyze_array_type(c, node);
        case AST_ARRAY_TYPE_SUGAR: return analyze_array_type_sugar(c, node);
        case AST_POINTER_MUT_TYPE: return analyze_mut_ptr_type(c, node);
        case AST_SLICE_TYPE: return analyze_slice_type(c, node);
        case AST_SLICE_MUT_TYPE: return analyze_mut_slice_type(c, node);
        case AST_FUNCTION_TYPE: return analyze_function_type(c, node);
        case AST_ID: return analyze_id(c, node);
        case AST_INT: return analyze_int(c, node, hint);
        case AST_FLOAT: return analyze_float(c, node, hint);
        case AST_CHAR: return analyze_char(c, node);
        case AST_STRING: return analyze_string(c, node);
        case AST_BOOL: return analyze_bool(c, node);
        case AST_NULL: return analyze_null(c, hint);
        case AST_PLUS: return analyze_un_arithmetic(c, node, hint, TIR_PLUS);
        case AST_MINUS: return analyze_un_arithmetic(c, node, hint, TIR_MINUS);
        case AST_NOT: return analyze_not(c, node);
        case AST_ADDRESS: return analyze_address(c, node, hint);
        case AST_DEREF: return analyze_deref(c, node);
        case AST_ADD: return analyze_bin_arithmetic(c, node, hint, TIR_ADD);
        case AST_SUB: return analyze_bin_arithmetic(c, node, hint, TIR_SUB);
        case AST_MUL: return analyze_bin_arithmetic(c, node, hint, TIR_MUL);
        case AST_DIV: return analyze_bin_arithmetic(c, node, hint, TIR_DIV);
        case AST_MOD: return analyze_bin_arithmetic(c, node, hint, TIR_MOD);
        case AST_AND: return analyze_bin_bit(c, node, hint, TIR_AND);
        case AST_OR: return analyze_bin_bit(c, node, hint, TIR_OR);
        case AST_XOR: return analyze_bin_bit(c, node, hint, TIR_XOR);
        case AST_SHL: return analyze_bin_bit(c, node, hint, TIR_SHL);
        case AST_SHR: return analyze_bin_bit(c, node, hint, TIR_SHR);
        case AST_LOGIC_AND: return analyze_logic(c, node, true);
        case AST_LOGIC_OR: return analyze_logic(c, node, false);
        case AST_EQ: return analyze_eq(c, node, TIR_EQ);
        case AST_NE: return analyze_eq(c, node, TIR_NE);
        case AST_LT: return analyze_rel(c, node, TIR_LT);
        case AST_GT: return analyze_rel(c, node, TIR_GT);
        case AST_LE: return analyze_rel(c, node, TIR_LE);
        case AST_GE: return analyze_rel(c, node, TIR_GE);
        case AST_ASSIGN: return analyze_assign(c, node);
        case AST_ASSIGN_ADD: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_ADD);
        case AST_ASSIGN_SUB: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_SUB);
        case AST_ASSIGN_MUL: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_MUL);
        case AST_ASSIGN_DIV: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_DIV);
        case AST_ASSIGN_MOD: return analyze_assign_arithmetic(c, node, TIR_ASSIGN_MOD);
        case AST_ASSIGN_AND: return analyze_assign_bit(c, node, TIR_ASSIGN_AND);
        case AST_ASSIGN_OR: return analyze_assign_bit(c, node, TIR_ASSIGN_OR);
        case AST_ASSIGN_XOR: return analyze_assign_bit(c, node, TIR_ASSIGN_XOR);
        case AST_ACCESS: return analyze_access(c, node);
        case AST_INFERRED_ACCESS: return analyze_enum_member_inferred(c, node, hint);
        case AST_TYPE_HINT: return analyze_type_hint(c, node);
        case AST_CALL: return analyze_call(c, node);
        case AST_INDEX: return analyze_index(c, node, hint);
        case AST_SLICE: return analyze_slice(c, node);
        case AST_LIST: return analyze_list(c, node, hint);
        case AST_SWITCH: return analyze_switch(c, node, hint);

        case AST_LET: return analyze_let(c, node, false);
        case AST_MUT: return analyze_let(c, node, true);
        case AST_CONST: return analyze_const(c, node);
        case AST_IF: return analyze_if(c, node);
        case AST_WHILE: return analyze_while(c, node);
        case AST_FOR_HELPER: return analyze_for_helper(c, node);
        case AST_FOR: return analyze_for(c, node);
        case AST_BREAK: return analyze_break(c, node);
        case AST_CONTINUE: return analyze_continue(c, node);
        case AST_RETURN: return analyze_return(c, get_ast_unary(node, c->ast));

        case AST_IMPORT: return analyze_import(c, node);
        case AST_FUNCTION: return analyze_function_decl(c, node);
        case AST_ENUM: return analyze_enum(c, node);
        case AST_STRUCT: return analyze_struct(c, node);
        case AST_NEWTYPE: return analyze_newtype(c, node);
        case AST_EXTERN_FUNCTION: return analyze_extern_function(c, node);
        case AST_EXTERN_MUT: return analyze_extern_mut(c, node);

        default: abort();
    }
}

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch) {
    Tir global_tir = {0};
    Context global_tc = {0};
    global_tc.options = input->options;
    global_tc.paths = input->paths;
    global_tc.sources = input->sources;
    global_tc.asts = input->asts;
    global_tc.files = input->files;
    global_tc.module_table = input->module_table;
    global_tc.modules = input->modules;
    global_tc.global_scope = input->global_scope;
    global_tc.ast_refs = input->ast_refs;
    global_tc.permanent = permanent;
    global_tc.scratch = &scratch;
    global_tc.rirs = arena_alloc(&scratch, Role, input->def_count);
    global_tc.tir_refs = arena_alloc(&scratch, TirId, input->def_count);
    global_tc.tir.global = &global_tir;
    LocalTir *tirs = arena_alloc(permanent, LocalTir, input->function_count);
    global_tc.local_tirs = tirs;
    global_tc.module_import_notes = arena_alloc(&scratch, bool, input->module_table->count);

    for (int32_t i = 0; i < input->def_count; i++) {
        analyze_def(&global_tc, (DefId) {i});
    }

    int err = global_tc.error;

    #pragma omp parallel
    {
        Arena thread_base_scratch = new_arena(64 << 20);
        Arena thread_scratch = thread_base_scratch;
        Context local_tc = {0};
        local_tc.options = global_tc.options;
        local_tc.paths = global_tc.paths;
        local_tc.sources = global_tc.sources;
        local_tc.asts = global_tc.asts;
        local_tc.files = input->files;
        local_tc.module_table = input->module_table;
        local_tc.modules = input->modules;
        local_tc.global_scope = input->global_scope;
        local_tc.ast_refs = input->ast_refs;
        local_tc.permanent = permanent;
        local_tc.scratch = &thread_scratch;
        local_tc.rirs = global_tc.rirs;
        local_tc.tir_refs = global_tc.tir_refs;
        local_tc.tir.global = &global_tir;
        local_tc.local_tirs = tirs;

        #pragma omp for reduction (||:err)
        for (int32_t i = 0; i < input->function_count; i++) {
            DefId def = input->functions[i];
            TirId value = global_tc.tir_refs[def.id];
            AstRef ref = input->ast_refs[def.id];

            local_tc.file = ref.file;
            local_tc.ast = &input->asts[ref.file];
            local_tc.tir.thread = &tirs[i].deps;
            local_tc.local_tir = &tirs[i];

            analyze_function(&local_tc, ref.node, value);

            if (local_tc.error) {
                err = 1;
            }
        }

        delete_arena(&thread_base_scratch);
    }

    return (TirOutput) {
        .global_deps = global_tir,
        .insts = tirs,
        .error = err,
    };
}
