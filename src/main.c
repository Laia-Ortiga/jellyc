#include "adt.h"
#include "arena.h"
#include "data/ast.h"
#include "data/tir.h"
#include "diagnostic.h"
#include "fwd.h"
#include "gen.h"
#include "hash.h"
#include "lex.h"
#include "parse.h"
#include "print.h"
#include "role-analysis.h"
#include "tir-analysis.h"
#include "tir2mir.h"
#include "type-analysis.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help(void) {
    fprintf(stderr, "Usage: jellyc [options] file...\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -help                    Display this information.\n");
    fprintf(stderr, "  -print-debug             Display debug information about the intermediate representations.\n");
    fprintf(stderr, "  -backend=<backend>       Specify the backend that will be used.\n");
}

static Backend parse_backend(String value) {
    static struct { String value; Backend backend; } backends[] = {
        {Str("c"), BACKEND_C},
        {Str("llvm"), BACKEND_LLVM},
    };

    for (int i = 0; i < ArrayLength(backends); i++) {
        if (equals(backends[i].value, value)) {
            return backends[i].backend;
        }
    }

    fprintf(stderr, "unknown value for backend ");
    fwrite(value.ptr, 1, value.len, stderr);
    fprintf(stderr, "\n");
    return BACKEND_C;
}

static void print_tokens(char const *path, String source) {
    Lexer lexer = new_lexer(source);
    printf("Tokens(%s) {\n", path);

    for (;;) {
        Token token = next_token(&lexer);

        if (token.tag == TOK_SENTINEL) {
            break;
        }

        printf(
            "  %.*s %s\n",
            (int) (token.end.index - token.start.index),
            lexer.source.ptr + token.start.index,
            token_tag_to_string(token.tag)
        );
    }

    printf("}\n");
}

static String read_file(char const *path) {
    FILE *file = fopen(path, "r");
    String buffer = {0};

    if (!file) {
        return buffer;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length >= 0) {
        char *data = malloc(length);

        if (data) {
            if (fread(data, 1, (size_t) length, file) == (size_t) length) {
                buffer.ptr = data;
                buffer.len = length;
            } else {
                free(data);
            }
        }
    }

    fclose(file);
    return buffer;
}

typedef struct {
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    Module *modules;
    HashTable *global_scope;
    HashTable *extern_symbols;
    AstRefVec *ast_refs;
    DefVec *functions;
} GlobalScopeBuilder;

static SourceLoc get_ast_location(GlobalScopeBuilder *b, AstRef def) {
    SourceIndex token = get_ast_token(def.node, &b->asts[def.file]);
    String name = id_token_to_string(b->sources[def.file], token);
    return (SourceLoc) {
        .path = b->paths[def.file],
        .source = b->sources[def.file],
        .where = token,
        .len = name.len,
        .mark = token,
    };
}

static String get_string_from_location(SourceLoc const *loc) {
    return substring(loc->source, loc->where.index, loc->where.index + loc->len);
}

static Symbol lookup(GlobalScopeBuilder *b, int32_t file, String name) {
    uint32_t *file_def = htable_lookup(&b->files[file].scope, name);
    if (file_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*file_def}};
    }

    int32_t module = b->files[file].module;

    uint32_t *private_def = htable_lookup(&b->modules[module].private_scope, name);
    if (private_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*private_def}};
    }

    uint32_t *public_def = htable_lookup(&b->modules[module].public_scope, name);
    if (public_def) {
        return (Symbol) {.kind = SYM_GLOBAL, .global = {*public_def}};
    }

    uint32_t *builtin_def = htable_lookup(b->global_scope, name);
    if (builtin_def) {
        return (Symbol) {.kind = SYM_BUILTIN, .global = {*builtin_def}};
    }

    return (Symbol) {0};
}

static int add_global(GlobalScopeBuilder *b, AstRef def) {
    int32_t module = b->files[def.file].module;
    Ast *ast = &b->asts[def.file];
    HashTable *scope = &b->modules[module].private_scope;
    if (get_ast_tag(def.node, ast) == AST_PUBLIC) {
        scope = &b->modules[module].public_scope;
        def.node = get_ast_unary(def.node, ast);
    }

    bool is_extern = false;
    bool is_function = false;
    switch (get_ast_tag(def.node, ast)) {
        case AST_IMPORT: {
            scope = &b->files[def.file].scope;
            break;
        }
        case AST_FUNCTION: {
            is_function = true;
            break;
        }
        case AST_STRUCT:
        case AST_ENUM:
        case AST_NEWTYPE:
        case AST_CONST: {
            break;
        }
        case AST_EXTERN_FUNCTION:
        case AST_EXTERN_MUT: {
            is_extern = true;
            break;
        }
        default: {
            return 0;
        }
    }

    SourceLoc loc = get_ast_location(b, def);
    String name = get_string_from_location(&loc);

    uint32_t *prev_extern_sym = is_extern ? htable_lookup(b->extern_symbols, name) : NULL;
    if (prev_extern_sym) {
        print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_EXTERN_DEFINITION});
        AstRef prev_ref = b->ast_refs->ptr[*prev_extern_sym];
        SourceLoc prev_loc = get_ast_location(b, prev_ref);
        print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
        return 1;
    }

    Symbol prev_sym = lookup(b, def.file, name);
    if (prev_sym.kind != SYM_UNDEFINED) {
        print_diagnostic(&loc, &(Diagnostic) {.kind = ERROR_MULTIPLE_DEFINITION});
        if (prev_sym.kind == SYM_GLOBAL) {
            AstRef prev_ref = b->ast_refs->ptr[prev_sym.global.id];
            SourceLoc prev_loc = get_ast_location(b, prev_ref);
            print_diagnostic(&prev_loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_DEFINITION});
        } else {
            print_diagnostic(&loc, &(Diagnostic) {.kind = NOTE_PREVIOUS_BUILTIN_DEFINITION});
        }
        return 1;
    }

    int32_t entry = b->ast_refs->len;
    vec_push(b->ast_refs, def);
    htable_try_insert(scope, name, entry);
    if (is_extern) {
        htable_try_insert(b->extern_symbols, name, entry);
    }
    if (is_function) {
        vec_push(b->functions, (DefId) {entry});
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_help();
    }

    Options options = {0};
    int o;

    for (o = 1; o < argc && argv[o][0] == '-'; o++) {
        String arg = {strlen(argv[o]), argv[o]};
        char const *eq = memchr(arg.ptr, '=', arg.len);

        if (eq) {
            String key = substring(arg, 1, eq - arg.ptr);
            String value = substring(arg, (eq + 1) - arg.ptr, arg.len);

            if (equals(key, (String) Str("backend"))) {
                options.backend = parse_backend(value);
                continue;
            }

            fprintf(stderr, "ignored unknown argument ");
            fwrite(key.ptr, 1, key.len, stderr);
            fprintf(stderr, "\n");
        } else {
            String option = substring(arg, 1, arg.len);

            if (equals(option, (String) Str("help"))) {
                print_help();
                continue;
            }

            if (equals(option, (String) Str("print-debug"))) {
                options.print_debug = true;
                continue;
            }

            fprintf(stderr, "ignored unknown option ");
            fwrite(option.ptr, 1, option.len, stderr);
            fprintf(stderr, "\n");
        }
    }

    int file_count = argc - o;
    char **paths = argv + o;

    Arena permanent_arena = new_arena(64 << 20);
    Arena scratch_arena = new_arena(64 << 20);

    if (init_lex_module()) {
        abort();
    }
    String *sources = arena_alloc(&permanent_arena, String, file_count);
    for (int i = 0; i < file_count; i++) {
        String source = read_file(paths[i]);
        sources[i] = source;
        if (!source.len) {
            fprintf(stderr, "failed to read file \"%s\"\n", paths[i]);
        }
    }

    if (options.print_debug) {
        for (int i = 0; i < file_count; i++) {
            if (sources[i].len) {
                print_tokens(paths[i], sources[i]);
            }
        }
    }

    init_diagnostic_module();
    Ast *asts = arena_alloc(&permanent_arena, Ast, file_count);
    int err = 0;
    #pragma omp parallel for reduction (||:err)
    for (int i = 0; i < file_count; i++) {
        String source = sources[i];
        if (!source.len || parse_ast(&asts[i], paths[i], source)) {
            err = 1;
        }
    }
    if (err) {
        return -1;
    }
    if (options.print_debug) {
        for (int i = 0; i < file_count; i++) {
            print_ast(paths[i], sources[i], &asts[i]);
        }
    }

    File *files = arena_alloc(&permanent_arena, File, file_count);
    HashTable module_table = htable_init();
    for (int32_t i = 0; i < file_count; i++) {
        SourceIndex module_token = get_ast_token(null_ast, &asts[i]);
        String module_name = id_token_to_string(sources[i], module_token);
        int32_t new_module = module_table.count;
        int64_t module = htable_try_insert(&module_table, module_name, new_module);
        if (module < 0) {
            module = new_module;
        }
        files[i].module = module;
        files[i].scope = htable_init();
    }

    Module *modules = arena_alloc(&permanent_arena, Module, module_table.count);
    for (int32_t i = 0; i < (int32_t) module_table.count; i++) {
        modules[i].public_scope = htable_init();
        modules[i].private_scope = htable_init();
    }

    HashTable global_scope = htable_init();
    #define TYPE(type) htable_try_insert(&global_scope, (String) Str(#type), BUILTIN_##type);
    #include "simple-types"
    htable_try_insert(&global_scope, (String) Str("`Size"), BUILTIN_SIZE_TAG);
    htable_try_insert(&global_scope, (String) Str("`Alignment"), BUILTIN_ALIGNMENT_TAG);
    htable_try_insert(&global_scope, (String) Str("`align_of"), BUILTIN_ALIGNOF);
    htable_try_insert(&global_scope, (String) Str("`size_of"), BUILTIN_SIZEOF);
    htable_try_insert(&global_scope, (String) Str("`zero_extend"), BUILTIN_ZERO_EXTEND);
    htable_try_insert(&global_scope, (String) Str("`slice"), BUILTIN_SLICE);
    htable_try_insert(&global_scope, (String) Str("`Affine"), BUILTIN_AFFINE);
    htable_try_insert(&global_scope, (String) Str("`ArrayLength"), BUILTIN_ARRAY_LENGTH_TYPE);

    AstRefVec ast_refs = {0};
    DefVec functions = {0};
    {
        HashTable extern_symbols = htable_init();
        GlobalScopeBuilder b = {0};
        b.paths = paths;
        b.sources = sources;
        b.asts = asts;
        b.files = files;
        b.modules = modules;
        b.global_scope = &global_scope;
        b.extern_symbols = &extern_symbols;
        b.ast_refs = &ast_refs;
        b.functions = &functions;
        for (int32_t i = 0; i < file_count; i++) {
            AstList list = get_ast_list(null_ast, &asts[i]);
            for (int32_t j = 0; j < list.count; j++) {
                add_global(&b, (AstRef) {list.nodes[j], i});
            }
        }
        htable_free(&extern_symbols);
    }

    Rir *rirs = arena_alloc(&permanent_arena, Rir, file_count);
    for (int32_t i = 0; i < file_count; i++) {
        rirs[i].tags = arena_alloc(&permanent_arena, unsigned char, asts[i].nodes.len);
        rirs[i].data = arena_alloc(&permanent_arena, int32_t, asts[i].nodes.len);
    }

    RirTopInput rir_input = {0};
    rir_input.file_count = file_count;
    rir_input.paths = paths;
    rir_input.sources = sources;
    rir_input.asts = asts;
    rir_input.files = files;
    rir_input.module_table = &module_table;
    rir_input.modules = modules;
    rir_input.global_scope = &global_scope;
    rir_input.rirs = rirs;
    rir_input.ast_refs = ast_refs.ptr;
    rir_input.def_count = ast_refs.len;
    rir_input.functions = functions.ptr;
    rir_input.function_count = functions.len;
    RirTopOutput rir_output = analyze_roles(&rir_input, &permanent_arena, scratch_arena);

    TirInput tir_input = {0};
    tir_input.options = &options;
    tir_input.paths = paths;
    tir_input.sources = sources;
    tir_input.asts = asts;
    tir_input.files = files;
    tir_input.module_table = &module_table;
    tir_input.modules = modules;
    tir_input.global_scope = &global_scope;
    tir_input.ast_refs = ast_refs.ptr;
    tir_input.def_count = ast_refs.len;
    tir_input.order_count = rir_output.count;
    tir_input.local_ast_refs = rir_output.local_ast_refs;
    tir_input.order = rir_output.order;
    tir_input.rirs = rirs;
    tir_input.functions = functions.ptr;
    tir_input.function_count = functions.len;
    TirOutput tir_output = analyze_types(&tir_input, &permanent_arena, scratch_arena);
    if (rir_output.error || tir_output.error) {
        return -1;
    }
    if (options.print_debug) {
        for (int32_t i = 0; i < tir_output.declarations.functions.len; i++) {
            TirContext ctx = {
                .global = &tir_output.global_deps,
                .thread = &tir_output.insts[i],
            };
            int32_t name = get_value_data(ctx, tir_output.declarations.functions.ptr[i])->index;
            print_tir(ctx, &ctx.global->strtab.ptr[name], &tir_output.insts[i].insts, tir_output.insts[i].first);
        }
    }
    err = check_substructural_types(
        &(SubstructuralAnalysisInput) {
            .paths = paths,
            .sources = sources,
            .asts = asts,
            .ast_refs = ast_refs.ptr,
            .global_deps = &tir_output.global_deps,
            .insts = tir_output.insts,
            .functions = functions.ptr,
            .function_count = functions.len,
        },
        scratch_arena
    );
    if (err) {
        return -1;
    }

    MirResult mir_result = tir_to_mir(&(MirAnalysisInput) {
        .paths = paths,
        .sources = sources,
        .asts = asts,
        .ast_refs = ast_refs.ptr,
        .functions = tir_output.declarations.functions.ptr,
        .global_deps = &tir_output.global_deps,
        .insts = tir_output.insts,
        .function_count = tir_output.declarations.functions.len,
    }, &permanent_arena, scratch_arena);
    GenInput gen_input = {
        .declarations = tir_output.declarations,
        .global_deps = tir_output.global_deps,
        .insts = tir_output.insts,
        .mir_result = &mir_result,
    };
    switch (options.backend) {
        case BACKEND_C: {
            gen_c(&gen_input, options.target);
            break;
        }
        case BACKEND_LLVM: {
            gen_llvm(&gen_input, options.target, scratch_arena);
            break;
        }
    }
}
