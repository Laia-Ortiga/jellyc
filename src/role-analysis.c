#include "role-analysis.h"

#include "data/ast.h"
#include "data/rir.h"
#include "diagnostic.h"
#include "fwd.h"
#include "hash.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Scope {
    struct Scope *parent;
    HashTable table;
} Scope;

typedef struct {
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    HashTable *module_table;
    Module *modules;
    HashTable *global_scope;
    AstRef *ast_refs;
    Rir *rirs;
    Arena *scratch;

    bool *module_import_notes;
    unsigned char *rir_refs;
    Locals *local_ast_refs;
    Scope *scope;
    DefId *order;
    int32_t count;
    int error;
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
    uint32_t *file_def = htable_lookup(&c->files[file].scope, name);
    if (file_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*file_def}};
    }

    int32_t module = c->files[file].module;

    uint32_t *private_def = htable_lookup(&c->modules[module].private_scope, name);
    if (private_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*private_def}};
    }

    uint32_t *public_def = htable_lookup(&c->modules[module].public_scope, name);
    if (public_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*public_def}};
    }

    uint32_t *builtin_def = htable_lookup(c->global_scope, name);
    if (builtin_def) {
        return (Symbol) {.kind = SYM_BUILTIN, .global = {*builtin_def}};
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
        uint32_t *symbol = htable_lookup(&scope->table, name);
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

static void set_rir(Context *c, AstRef ref, RirTag tag, int32_t data) {
    c->rirs[ref.file].tags[ref.node.private_field_id] = tag;
    c->rirs[ref.file].data[ref.node.private_field_id] = data;
}

static void add_local(Context *c, AstRef ref, Role role) {
    String name = get_id_source(c, ref);
    Symbol prev_symbol = find_symbol(c, ref.file, name);
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
                LocalAstRef info = c->local_ast_refs[ref.file].ptr[prev_symbol.local.id];
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
    int32_t sym = c->local_ast_refs[ref.file].len;
    LocalAstRef local_ref = {role, ref.node};
    vec_push(&c->local_ast_refs[ref.file], local_ref);
    htable_try_insert(&c->scope->table, name, sym);
    c->rirs[ref.file].data[ref.node.private_field_id] = sym;
}

static Role analyze_node(Context *c, AstRef node);
static void analyze_statement(Context *c, AstRef ref);

static AstRef subvertex(AstRef ref, AstId ast_id) {
    return (AstRef) {ast_id, ref.file};
}

static void expect_type(Context *c, AstRef ref, ErrorKind kind) {
    switch (analyze_node(c, ref)) {
        case ROLE_NOT_VISITED: break;
        case ROLE_VISITING: break;
        case ROLE_INVALID: break;
        case ROLE_TYPE: break;
        case ROLE_TAG_TYPE: break;
        default: diagnostic(c, ref, kind); break;
    }
}

static void expect_value(Context *c, AstRef ref) {
    switch (analyze_node(c, ref)) {
        case ROLE_NOT_VISITED: break;
        case ROLE_VISITING: break;
        case ROLE_INVALID: break;
        case ROLE_VALUE: break;
        default: diagnostic(c, ref, ERROR_EXPECTED_VALUE); break;
    }
}

typedef struct {
    Role role;
    RirTag tag;
    int32_t data;
} Result;

static Result analyze_import(Context *c, AstRef ref) {
    String name = get_id_source(c, ref);
    uint32_t *module = htable_lookup(c->module_table, name);

    if (!module) {
        diagnostic(c, ref, ERROR_UNDEFINED_MODULE);
        return (Result) {ROLE_INVALID, RIR_ROOT, 0};
    }

    return (Result) {ROLE_MODULE, RIR_ROOT, *module};
}

static void analyze_param(Context *c, AstRef ref) {
    AstId param_type = get_ast_unary(ref.node, &c->asts[ref.file]);
    expect_type(c, subvertex(ref, param_type), ERROR_EXPECTED_TYPE);
}

static Result analyze_function_decl(Context *c, AstRef ref) {
    AstFunction f = get_ast_function(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < f.type_param_count; i++) {
        add_local(c, subvertex(ref, f.type_params[i]), ROLE_TYPE);
    }
    for (int32_t i = 0; i < f.param_count; i++) {
        AstRef param = subvertex(ref, f.params[i]);
        analyze_param(c, param);
        add_local(c, param, ROLE_VALUE);
    }
    if (!is_ast_null(f.ret)) {
        expect_type(c, subvertex(ref, f.ret), ERROR_EXPECTED_TYPE);
    }
    return (Result) {ROLE_VALUE, RIR_FUNCTION, 0};
}

static Result analyze_struct(Context *c, AstRef ref) {
    AstStruct s = get_ast_struct(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < s.type_param_count; i++) {
        add_local(c, subvertex(ref, s.type_params[i]), ROLE_TYPE);
    }
    for (int32_t i = 0; i < s.field_count; i++) {
        analyze_param(c, subvertex(ref, s.fields[i]));
    }
    return (Result) {ROLE_TYPE, RIR_STRUCT, 0};
}

static Result analyze_enum(Context *c, AstRef ref) {
    AstEnum e = get_ast_enum(ref.node, &c->asts[ref.file]);
    expect_type(c, subvertex(ref, e.repr), ERROR_EXPECTED_TYPE);
    return (Result) {ROLE_TYPE, RIR_ENUM, 0};
}

static Result analyze_newtype(Context *c, AstRef ref) {
    AstNewtype n = get_ast_newtype(ref.node, &c->asts[ref.file]);
    expect_type(c, subvertex(ref, n.type), ERROR_EXPECTED_TYPE);
    return (Result) {ROLE_TAG_TYPE, RIR_NEWTYPE, 0};
}

static Result analyze_const(Context *c, AstRef ref) {
    AstId init = get_ast_unary(ref.node, &c->asts[ref.file]);
    Role role = analyze_node(c, subvertex(ref, init));
    switch (role) {
        case ROLE_INVALID:
        case ROLE_BUILTIN_MACRO:
        case ROLE_MACRO: {
            return (Result) {role, RIR_ROOT, 0};
        }
        case ROLE_TYPE: {
            return (Result) {role, RIR_TYPE_ALIAS, 0};
        }
        case ROLE_VALUE: {
            return (Result) {role, RIR_CONST, 0};
        }
        default: {
            diagnostic(c, ref, ERROR_CONST_INIT);
            return (Result) {ROLE_INVALID, RIR_ROOT, 0};
        }
    }
}

static Result analyze_extern_function(Context *c, AstRef ref) {
    AstFunction f = get_ast_extern_function(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < f.param_count; i++) {
        analyze_param(c, subvertex(ref, f.params[i]));
    }
    if (!is_ast_null(f.ret)) {
        expect_type(c, subvertex(ref, f.ret), ERROR_EXPECTED_TYPE);
    }
    return (Result) {ROLE_VALUE, RIR_EXTERN_FUNCTION, 0};
}

static Result analyze_extern_mut(Context *c, AstRef ref) {
    analyze_param(c, ref);
    return (Result) {ROLE_VALUE, RIR_EXTERN_MUT, 0};
}

static int analyze_def(Context *c, DefId def) {
    Role prev_role = c->rir_refs[def.id];
    if (prev_role != ROLE_NOT_VISITED && prev_role != ROLE_VISITING) {
        return 0;
    }
    AstRef ref = c->ast_refs[def.id];
    if (prev_role == ROLE_VISITING) {
        diagnostic(c, ref, ERROR_RECURSIVE_DEPENDENCY);
        return 1;
    }
    c->rir_refs[def.id] = ROLE_VISITING;
    Result result = {0};
    switch (get_ast_tag(ref.node, &c->asts[ref.file])) {
        case AST_IMPORT: result = analyze_import(c, ref); break;
        case AST_FUNCTION: result = analyze_function_decl(c, ref); break;
        case AST_STRUCT: result = analyze_struct(c, ref); break;
        case AST_ENUM: result = analyze_enum(c, ref); break;
        case AST_NEWTYPE: result = analyze_newtype(c, ref); break;
        case AST_CONST: result = analyze_const(c, ref); break;
        case AST_EXTERN_FUNCTION: result = analyze_extern_function(c, ref); break;
        case AST_EXTERN_MUT: result = analyze_extern_mut(c, ref); break;
        default: break;
    }
    c->rir_refs[def.id] = result.role;
    set_rir(c, ref, result.tag, result.data);
    if (result.role != ROLE_INVALID && c->order) {
        c->order[c->count++] = def;
    }
    return 0;
}

static void analyze_let(Context *c, AstRef ref, RirTag tag) {
    AstId init = get_ast_unary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, init));
    set_rir(c, ref, tag, 0);
    add_local(c, ref, ROLE_VALUE);
}

static void analyze_local_const(Context *c, AstRef ref) {
    Result result = analyze_const(c, ref);
    set_rir(c, ref, result.tag, result.data);
    add_local(c, ref, result.role);
}

static Role analyze_function_type(Context *c, AstRef ref) {
    AstCall s = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < s.arg_count; i++) {
        analyze_param(c, subvertex(ref, s.args[i]));
    }
    if (!is_ast_null(s.operand)) {
        expect_type(c, subvertex(ref, s.operand), ERROR_EXPECTED_TYPE);
    }
    set_rir(c, ref, RIR_FUNCTION_TYPE, 0);
    return ROLE_TYPE;
}

static Role analyze_array_type(Context *c, AstRef ref) {
    AstBinary bin = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_type(c, subvertex(ref, bin.left), ERROR_ARRAY_TYPE_EXPECTS_LENGTH_TYPE);
    expect_type(c, subvertex(ref, bin.right), ERROR_EXPECTED_TYPE);
    set_rir(c, ref, RIR_ARRAY_TYPE, 0);
    return ROLE_TYPE;
}

static Role analyze_array_type_sugar(Context *c, AstRef ref) {
    AstBinary bin = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, bin.left));
    expect_type(c, subvertex(ref, bin.right), ERROR_EXPECTED_TYPE);
    set_rir(c, ref, RIR_ARRAY_TYPE_SUGAR, 0);
    return ROLE_TYPE;
}

static Role resolve_global(Context *c, AstRef ref, DefId global) {
    if (c->order && analyze_def(c, global)) {
        diagnostic(c, ref, NOTE_RECURSION);
    }
    if (c->rir_refs[global.id] == ROLE_MODULE) {
        set_rir(c, ref, RIR_GLOBAL_ID, get_rir_data(c->ast_refs[global.id].node, &c->rirs[c->ast_refs[global.id].file]));
        return ROLE_MODULE;
    }
    set_rir(c, ref, RIR_GLOBAL_ID, global.id);
    return c->rir_refs[global.id];
}

static Role analyze_id(Context *c, AstRef ref) {
    String name = get_id_source(c, ref);

    LocalId local = lookup_local(c, name);
    if (local.id) {
        LocalAstRef info = c->local_ast_refs[ref.file].ptr[local.id];
        if (info.role == ROLE_INVALID) {
            return ROLE_INVALID;
        }
        set_rir(c, ref, RIR_LOCAL_ID, local.id);
        return info.role;
    }

    Symbol symbol = lookup(c, ref.file, name);
    switch (symbol.kind) {
        case SYM_UNDEFINED: {
            diagnostic(c, ref, ERROR_UNDEFINED_NAME);

            // Check module names for hints.
            uint32_t *m = htable_lookup(c->module_table, name);
            if (m && !c->module_import_notes[*m]) {
                SourceLoc loc = get_ast_location(c, (AstRef) {ref.node, ref.file});
                print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_FORGOT_IMPORT});
                c->module_import_notes[*m] = true;
            }

            add_local(c, ref, ROLE_INVALID);
            break;
        }
        case SYM_BUILTIN: {
            set_rir(c, ref, RIR_BUILTIN_ID, symbol.builtin);
            switch (symbol.builtin) {
                #define TYPE(type) case BUILTIN_##type:
                #include "simple-types"
                {
                    return ROLE_TYPE;
                }
                case BUILTIN_SIZE_TAG:
                case BUILTIN_ALIGNMENT_TAG: {
                    return ROLE_TAG_TYPE;
                }
                case BUILTIN_ALIGNOF:
                case BUILTIN_SIZEOF:
                case BUILTIN_ZERO_EXTEND:
                case BUILTIN_SLICE:
                case BUILTIN_AFFINE:
                case BUILTIN_ARRAY_LENGTH_TYPE: {
                    return ROLE_BUILTIN_MACRO;
                }
            }
            break;
        }
        case SYM_GLOBAL: {
            return resolve_global(c, ref, symbol.global);
        }
        case SYM_LOCAL: {
            break;
        }
    }

    return ROLE_INVALID;
}

static Role analyze_int(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_INT, 0);
    return ROLE_VALUE;
}

static Role analyze_float(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_FLOAT, 0);
    return ROLE_VALUE;
}

static Role analyze_char(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_CHAR, 0);
    return ROLE_VALUE;
}

static Role analyze_string(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_STRING, 0);
    return ROLE_VALUE;
}

static Role analyze_bool(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_BOOL, 0);
    return ROLE_VALUE;
}

static Role analyze_null(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_NULL, 0);
    return ROLE_VALUE;
}

static Role analyze_value_unary(Context *c, AstRef ref, RirTag tag) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, operand));
    set_rir(c, ref, tag, 0);
    return ROLE_VALUE;
}

static Role analyze_address(Context *c, AstRef ref) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, operand))) {
        case ROLE_INVALID: return ROLE_INVALID;
        case ROLE_VALUE: {
            set_rir(c, ref, RIR_ADDRESS, 0);
            return ROLE_VALUE;
        }
        case ROLE_MULTIVALUE: {
            set_rir(c, ref, RIR_MULTIADDRESS, 0);
            return ROLE_VALUE;
        }
        default: diagnostic(c, ref, ERROR_EXPECTED_VALUE); return ROLE_INVALID;
    }
}

static Role analyze_ptr(Context *c, AstRef ref) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, operand))) {
        case ROLE_INVALID: return ROLE_INVALID;
        case ROLE_TYPE: {
            set_rir(c, ref, RIR_POINTER_TYPE, 0);
            return ROLE_TYPE;
        }
        case ROLE_VALUE: {
            set_rir(c, ref, RIR_DEREF, 0);
            return ROLE_VALUE;
        }
        default: diagnostic(c, ref, ERROR_DEREF_OPERAND_ROLE); return ROLE_INVALID;
    }
}

static Role analyze_ptr_type(Context *c, AstRef ref, RirTag tag) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    expect_type(c, subvertex(ref, operand), ERROR_EXPECTED_TYPE);
    set_rir(c, ref, tag, 0);
    return ROLE_TYPE;
}

static Role analyze_value_binary(Context *c, AstRef ref, RirTag tag) {
    AstBinary bin = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, bin.left));
    expect_value(c, subvertex(ref, bin.right));
    set_rir(c, ref, tag, 0);
    return ROLE_VALUE;
}

static Role analyze_assignment(Context *c, AstRef ref, RirTag tag) {
    AstBinary bin = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, bin.left));
    expect_value(c, subvertex(ref, bin.right));
    set_rir(c, ref, tag, 0);
    return ROLE_VALUE;
}

static Role analyze_module_id(Context *c, AstRef ref) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    int32_t module = get_rir_data(operand, &c->rirs[ref.file]);
    String name = get_id_source(c, ref);
    uint32_t *def_ptr = htable_lookup(&c->modules[module].public_scope, name);

    if (!def_ptr) {
        diagnostic(c, ref, ERROR_UNDEFINED_NAME_FROM_MODULE);

        // Check private namespace for hints.
        def_ptr = htable_lookup(&c->modules[module].private_scope, name);
        if (def_ptr) {
            AstRef ast_ref = c->ast_refs[*def_ptr];
            SourceLoc loc = get_ast_location(c, ast_ref);
            print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_PRIVATE_DEFINITION});
        }

        return ROLE_INVALID;
    }

    DefId global = {*def_ptr};
    return resolve_global(c, ref, global);
}

static Role analyze_access(Context *c, AstRef ref) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, operand))) {
        case ROLE_INVALID: return ROLE_INVALID;
        case ROLE_MODULE: return analyze_module_id(c, ref);
        case ROLE_TYPE: {
            set_rir(c, ref, RIR_SCOPE_ACCESS, 0);
            return ROLE_VALUE;
        }
        case ROLE_VALUE: {
            set_rir(c, ref, RIR_TYPE_ACCESS, 0);
            return ROLE_VALUE;
        }
        default: diagnostic(c, ref, ERROR_ACCESS_OPERAND_ROLE); return ROLE_INVALID;
    }
    return ROLE_INVALID;
}

static Role analyze_access_infer(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_INFERRED_SCOPE_ACCESS, 0);
    return ROLE_VALUE;
}

static Role analyze_cast(Context *c, AstRef ref) {
    AstBinary bin = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, bin.left));
    expect_type(c, subvertex(ref, bin.right), ERROR_EXPECTED_TYPE);
    set_rir(c, ref, RIR_CAST, 0);
    return ROLE_VALUE;
}

static Role analyze_value_args(Context *c, AstRef ref, RirTag tag) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_value(c, subvertex(ref, call.args[i]));
    }
    set_rir(c, ref, tag, 0);
    return ROLE_VALUE;
}

static Role analyze_call(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, call.operand))) {
        case ROLE_INVALID: return ROLE_INVALID;
        case ROLE_TYPE: return analyze_value_args(c, ref, RIR_CONSTRUCT);
        case ROLE_VALUE: return analyze_value_args(c, ref, RIR_CALL);
        default: diagnostic(c, ref, ERROR_CALL_OPERAND_ROLE); return ROLE_INVALID;
    }
}

static Role analyze_tagged_type(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_type(c, subvertex(ref, call.args[i]), ERROR_EXPECTED_TYPE);
    }
    set_rir(c, ref, RIR_TAG_TYPE, 0);
    return ROLE_TYPE;
}

static Role analyze_type_args_to_value(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_type(c, subvertex(ref, call.args[i]), ERROR_EXPECTED_TYPE);
    }
    return ROLE_VALUE;
}

static Role analyze_macro_value_args(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_value(c, subvertex(ref, call.args[i]));
    }
    return ROLE_VALUE;
}

static Role analyze_macro_type_args(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_type(c, subvertex(ref, call.args[i]), ERROR_EXPECTED_TYPE);
    }
    return ROLE_TYPE;
}

static Role analyze_value_args_to_type(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_value(c, subvertex(ref, call.args[i]));
    }
    return ROLE_TYPE;
}

static Role analyze_builtin_macro(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    BuiltinId macro = get_rir_data(call.operand, &c->rirs[ref.file]);
    set_rir(c, ref, RIR_CALL_BUILTIN, macro);
    switch (macro) {
        case BUILTIN_ALIGNOF:
        case BUILTIN_SIZEOF: return analyze_type_args_to_value(c, ref);
        case BUILTIN_ZERO_EXTEND:
        case BUILTIN_SLICE: return analyze_macro_value_args(c, ref);
        case BUILTIN_AFFINE: return analyze_macro_type_args(c, ref);
        case BUILTIN_ARRAY_LENGTH_TYPE: return analyze_value_args_to_type(c, ref);
        default: compiler_error("unknown built-in macro");
    }
}

static Role analyze_index(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, call.operand))) {
        case ROLE_INVALID: return ROLE_INVALID;
        case ROLE_BUILTIN_MACRO: return analyze_builtin_macro(c, ref);
        case ROLE_TAG_TYPE: return analyze_tagged_type(c, ref);

        case ROLE_VALUE:
        case ROLE_MULTIVALUE: return analyze_value_args(c, ref, RIR_INDEX);
        default: {
            diagnostic(c, ref, ERROR_INDEX_OPERAND_ROLE);
            return ROLE_INVALID;
        }
    }
}

static Role analyze_slice(Context *c, AstRef ref) {
    AstCall call = get_ast_call(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, call.operand));
    for (int32_t i = 0; i < call.arg_count; i++) {
        expect_value(c, subvertex(ref, call.args[i]));
    }
    set_rir(c, ref, RIR_SLICE, 0);
    return ROLE_MULTIVALUE;
}

static Role analyze_list(Context *c, AstRef ref) {
    AstList list = get_ast_list(ref.node, &c->asts[ref.file]);
    for (int32_t i = 0; i < list.count; i++) {
        expect_value(c, subvertex(ref, list.nodes[i]));
    }
    set_rir(c, ref, RIR_LIST, 0);
    return ROLE_VALUE;
}

static void analyze_block(Context *c, AstRef block, bool implicit_ret) {
    AstList list = get_ast_list(block.node, &c->asts[block.file]);
    for (int32_t i = 0; i < list.count; i++) {
        if (implicit_ret && i == list.count - 1) {
            if (get_ast_tag(list.nodes[i], &c->asts[block.file]) == AST_EXPRESSION_STATEMENT) {
                AstId expr = get_ast_unary(list.nodes[i], &c->asts[block.file]);
                expect_value(c, subvertex(block, expr));
                set_rir(c, subvertex(block, list.nodes[i]), RIR_RETURN, 0);
            } else {
                diagnostic(c, subvertex(block, list.nodes[i]), ERROR_EXPECTED_VALUE);
            }
        } else {
            analyze_statement(c, subvertex(block, list.nodes[i]));
        }
    }
}

static void analyze_if(Context *c, AstRef ref) {
    AstIf if_ = get_ast_if(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, if_.condition));
    push_scope(c);
    analyze_block(c, subvertex(ref, if_.true_block), false);
    pop_scope(c);
    push_scope(c);
    if (!is_ast_null(if_.false_block)) {
        analyze_block(c, subvertex(ref, if_.false_block), false);
    }
    pop_scope(c);
    set_rir(c, ref, RIR_IF, 0);
}

static void analyze_while(Context *c, AstRef ref) {
    AstBinary while_ = get_ast_binary(ref.node, &c->asts[ref.file]);
    expect_value(c, subvertex(ref, while_.left));
    push_scope(c);
    analyze_block(c, subvertex(ref, while_.right), false);
    pop_scope(c);
    set_rir(c, ref, RIR_WHILE, 0);
}

static void analyze_for_helper(Context *c, AstRef ref) {
    set_rir(c, ref, RIR_FOR_HELPER, 0);
}

static void analyze_for(Context *c, AstRef ref) {
    AstFor for_ = get_ast_for(ref.node, &c->asts[ref.file]);
    push_scope(c);
    analyze_statement(c, subvertex(ref, for_.init));
    analyze_node(c, subvertex(ref, for_.condition));
    analyze_node(c, subvertex(ref, for_.next));
    analyze_block(c, subvertex(ref, for_.block), false);
    pop_scope(c);
    set_rir(c, ref, RIR_FOR, 0);
}

static Role analyze_switch(Context *c, AstRef ref) {
    AstCall switch_ = get_ast_call(ref.node, &c->asts[ref.file]);

    if (!is_ast_null(switch_.operand)) {
        expect_value(c, subvertex(ref, switch_.operand));
    }

    for (int32_t i = 0; i < switch_.arg_count; i++) {
        AstBinary case_ = get_ast_binary(switch_.args[i], &c->asts[ref.file]);
        if (!is_ast_null(case_.left)) {
            expect_value(c, subvertex(ref, case_.left));
        }
        expect_value(c, subvertex(ref, case_.right));
    }

    set_rir(c, ref, RIR_SWITCH, 0);
    return ROLE_VALUE;
}

static void analyze_return(Context *c, AstRef ref) {
    AstId operand = get_ast_unary(ref.node, &c->asts[ref.file]);
    if (!is_ast_null(operand)) {
        expect_value(c, subvertex(ref, operand));
    }
    set_rir(c, ref, RIR_RETURN, 0);
}

static void analyze_expression_statement(Context *c, AstRef ref) {
    AstId expr = get_ast_unary(ref.node, &c->asts[ref.file]);
    switch (analyze_node(c, subvertex(ref, expr))) {
        case ROLE_TYPE: {
            set_rir(c, ref, RIR_TYPE_STATEMENT, 0);
            break;
        }
        case ROLE_VALUE:
        case ROLE_MULTIVALUE: {
            set_rir(c, ref, RIR_VALUE_STATEMENT, 0);
            break;
        }
        default: {
            break;
        }
    }
}

static Role analyze_node(Context *c, AstRef ref) {
    switch (get_ast_tag(ref.node, &c->asts[ref.file])) {
        case AST_FUNCTION_TYPE: return analyze_function_type(c, ref);
        case AST_ARRAY_TYPE: return analyze_array_type(c, ref);
        case AST_ARRAY_TYPE_SUGAR: return analyze_array_type_sugar(c, ref);
        case AST_ID: return analyze_id(c, ref);
        case AST_INT: return analyze_int(c, ref);
        case AST_FLOAT: return analyze_float(c, ref);
        case AST_CHAR: return analyze_char(c, ref);
        case AST_STRING: return analyze_string(c, ref);
        case AST_BOOL: return analyze_bool(c, ref);
        case AST_NULL: return analyze_null(c, ref);
        case AST_PLUS: return analyze_value_unary(c, ref, RIR_PLUS);
        case AST_MINUS: return analyze_value_unary(c, ref, RIR_MINUS);
        case AST_NOT: return analyze_value_unary(c, ref, RIR_NOT);
        case AST_ADDRESS: return analyze_address(c, ref);
        case AST_DEREF: return analyze_ptr(c, ref);
        case AST_POINTER_MUT_TYPE: return analyze_ptr_type(c, ref, RIR_MUTABLE_POINTER_TYPE);
        case AST_SLICE_TYPE: return analyze_ptr_type(c, ref, RIR_SLICE_TYPE);
        case AST_SLICE_MUT_TYPE: return analyze_ptr_type(c, ref, RIR_MUTABLE_SLICE_TYPE);
        case AST_ADD: return analyze_value_binary(c, ref, RIR_ADD);
        case AST_SUB: return analyze_value_binary(c, ref, RIR_SUB);
        case AST_MUL: return analyze_value_binary(c, ref, RIR_MUL);
        case AST_DIV: return analyze_value_binary(c, ref, RIR_DIV);
        case AST_MOD: return analyze_value_binary(c, ref, RIR_MOD);
        case AST_AND: return analyze_value_binary(c, ref, RIR_AND);
        case AST_OR: return analyze_value_binary(c, ref, RIR_OR);
        case AST_XOR: return analyze_value_binary(c, ref, RIR_XOR);
        case AST_SHL: return analyze_value_binary(c, ref, RIR_SHL);
        case AST_SHR: return analyze_value_binary(c, ref, RIR_SHR);
        case AST_LOGIC_AND: return analyze_value_binary(c, ref, RIR_LOGIC_AND);
        case AST_LOGIC_OR: return analyze_value_binary(c, ref, RIR_LOGIC_OR);
        case AST_EQ: return analyze_value_binary(c, ref, RIR_EQ);
        case AST_NE: return analyze_value_binary(c, ref, RIR_NE);
        case AST_LT: return analyze_value_binary(c, ref, RIR_LT);
        case AST_GT: return analyze_value_binary(c, ref, RIR_GT);
        case AST_LE: return analyze_value_binary(c, ref, RIR_LE);
        case AST_GE: return analyze_value_binary(c, ref, RIR_GE);
        case AST_ASSIGN: return analyze_assignment(c, ref, RIR_ASSIGN);
        case AST_ASSIGN_ADD: return analyze_assignment(c, ref, RIR_ASSIGN_ADD);
        case AST_ASSIGN_SUB: return analyze_assignment(c, ref, RIR_ASSIGN_SUB);
        case AST_ASSIGN_MUL: return analyze_assignment(c, ref, RIR_ASSIGN_MUL);
        case AST_ASSIGN_DIV: return analyze_assignment(c, ref, RIR_ASSIGN_DIV);
        case AST_ASSIGN_MOD: return analyze_assignment(c, ref, RIR_ASSIGN_MOD);
        case AST_ASSIGN_AND: return analyze_assignment(c, ref, RIR_ASSIGN_AND);
        case AST_ASSIGN_OR: return analyze_assignment(c, ref, RIR_ASSIGN_OR);
        case AST_ASSIGN_XOR: return analyze_assignment(c, ref, RIR_ASSIGN_XOR);
        case AST_ACCESS: return analyze_access(c, ref);
        case AST_INFERRED_ACCESS: return analyze_access_infer(c, ref);
        case AST_CAST: return analyze_cast(c, ref);
        case AST_CALL: return analyze_call(c, ref);
        case AST_INDEX: return analyze_index(c, ref);
        case AST_SLICE: return analyze_slice(c, ref);
        case AST_LIST: return analyze_list(c, ref);
        case AST_SWITCH: return analyze_switch(c, ref);
        default: compiler_error_fmt("analyze_node: invalid ast tag %d", get_ast_tag(ref.node, &c->asts[ref.file]));
    }
}

static void analyze_statement(Context *c, AstRef ref) {
    switch (get_ast_tag(ref.node, &c->asts[ref.file])) {
        case AST_LET: analyze_let(c, ref, RIR_LET); break;
        case AST_MUT: analyze_let(c, ref, RIR_MUT); break;
        case AST_CONST: analyze_local_const(c, ref); break;
        case AST_IF: analyze_if(c, ref); break;
        case AST_WHILE: analyze_while(c, ref); break;
        case AST_FOR_HELPER: analyze_for_helper(c, ref); break;
        case AST_FOR: analyze_for(c, ref); break;
        case AST_BREAK: set_rir(c, ref, RIR_BREAK, 0); break;
        case AST_CONTINUE: set_rir(c, ref, RIR_CONTINUE, 0); break;
        case AST_RETURN: analyze_return(c, ref); break;
        case AST_EXPRESSION_STATEMENT: analyze_expression_statement(c, ref); break;
        default: compiler_error_fmt("analyze_statement: invalid ast tag %d", get_ast_tag(ref.node, &c->asts[ref.file]));
    }
}

RirTopOutput analyze_roles(RirTopInput *input, Arena *permanent, Arena scratch) {
    Context c = {0};
    c.paths = input->paths;
    c.sources = input->sources;
    c.asts = input->asts;
    c.files = input->files;
    c.module_table = input->module_table;
    c.modules = input->modules;
    c.global_scope = input->global_scope;
    c.rirs = input->rirs;
    c.ast_refs = input->ast_refs;
    c.scratch = &scratch;
    c.module_import_notes = arena_alloc(&scratch, bool, input->module_table->count);
    c.rir_refs = arena_alloc(permanent, unsigned char, input->def_count);
    c.order = arena_alloc(permanent, DefId, input->def_count);
    c.local_ast_refs = arena_alloc(permanent, Locals, input->file_count);
    Scope *scopes = arena_alloc(&scratch, Scope, input->def_count);
    for (int i = 0; i < input->file_count; i++) {
        vec_push(&c.local_ast_refs[i], (LocalAstRef) {0});
    }
    for (int32_t i = 0; i < input->def_count; i++) {
        scopes[i].table = htable_init();
        c.scope = &scopes[i];
        analyze_def(&c, (DefId) {i});
    }
    for (int32_t i = 0; i < input->function_count; i++) {
        DefId def = input->functions[i];
        c.scope = &scopes[def.id];
        AstRef ref = c.ast_refs[def.id];
        AstFunction f = get_ast_function(ref.node, &c.asts[ref.file]);
        analyze_block(&c, subvertex(ref, f.body), !is_ast_null(f.ret));
    }
    for (int32_t i = 0; i < input->def_count; i++) {
        htable_free(&scopes[i].table);
    }
    return (RirTopOutput) {
        .rir_refs = c.rir_refs,
        .order = c.order,
        .local_ast_refs = c.local_ast_refs,
        .count = c.count,
        .error = c.error,
    };
}
