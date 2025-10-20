#include "adt.h"
#include "arena.h"
#include "ast.h"
#include "tir.h"
#include "diagnostic.h"
#include "fwd.h"
#include "gen.h"
#include "hash.h"
#include "lex.h"
#include "parse.h"
#include "print.h"
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
    fprintf(stderr, "  -backend={c|llvm}        Specify the backend that will be used.\n");
}

static Backend parse_backend(String value) {
    static struct { String value; Backend backend; } const backends[] = {
        {Str2("c"), BACKEND_C},
        {Str2("llvm"), BACKEND_LLVM},
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
        char *data = malloc(length + 1);

        if (data) {
            if (fread(data, 1, (size_t) length, file) == (size_t) length) {
                data[length] = 0;
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
    Paths paths;
    Sources sources;
    Asts asts;
    Files files;
    Modules modules;
    HashTable *global_scope;
    HashTable *extern_symbols;
    AstRefVec *ast_refs;
    int32_t *function_body_count;
} GlobalScopeBuilder;

static SourceLoc get_ast_location(GlobalScopeBuilder *b, AstRef def) {
    SourceIndex token = get_ast_token(&nth(b->asts, def.file), def.node);
    String name = id_token_to_string(nth(b->sources, def.file), token);
    return (SourceLoc) {
        .path = nth(b->paths, def.file),
        .source = nth(b->sources, def.file),
        .where = token,
        .len = name.len,
        .mark = token,
    };
}

static String get_string_from_location(SourceLoc const *loc) {
    return substring(loc->source, loc->where.index, loc->where.index + loc->len);
}

static Symbol lookup(GlobalScopeBuilder *b, FileId file, String name) {
    Scopes scopes = {
        .files = b->files,
        .modules = b->modules,
        .reserved = b->global_scope,
    };
    return lookup_global(&scopes, file, name);
}

static int add_global(GlobalScopeBuilder *b, AstRef def) {
    ModuleId module = nth(b->files, def.file).module;
    Ast *ast = &nth(b->asts, def.file);
    HashTable *scope = &nth(b->modules, module).scope;
    int32_t is_public = 0;
    if (get_ast_tag(ast, def.node) == AST_PUBLIC) {
        is_public = -1;
        def.node = ast_get_public(ast, def.node).def;
    }

    bool is_extern = false;
    switch (get_ast_tag(ast, def.node)) {
        case AST_IMPORT: {
            scope = &nth(b->files, def.file).scope;
            break;
        }
        case AST_FUNCTION: {
            (*b->function_body_count)++;
            break;
        }
        case AST_STRUCT:
        case AST_ENUM:
        case AST_NEWTYPE:
        case AST_CONST: {
            break;
        }
        case AST_EXTERN_FUNCTION:
        case AST_EXTERN_VAR: {
            is_extern = true;
            break;
        }
        default: {
            return 0;
        }
    }

    SourceLoc loc = get_ast_location(b, def);
    String name = get_string_from_location(&loc);

    int32_t *prev_extern_sym = is_extern ? htable_lookup(b->extern_symbols, name) : NULL;
    if (prev_extern_sym) {
        GlobalId prev_extern_def = {*prev_extern_sym};
        print_diagnostic(&loc, &Diagnostic(ErrorMultipleExternDefinition, {0}));
        AstRef prev_ref = nth(b->ast_refs->table, prev_extern_def).ref;
        SourceLoc prev_loc = get_ast_location(b, prev_ref);
        print_diagnostic(&prev_loc, &Diagnostic(NotePreviousDefinition, {0}));
        return 1;
    }

    Symbol prev_sym = lookup(b, def.file, name);
    if (prev_sym.kind != SYM_UNDEFINED) {
        if (prev_sym.kind == SYM_PUBLIC_GLOBAL && prev_sym.global.private_field_id < RESERVED_INTERNAL_COUNT) {
            // Defined in "internal.jel".
            vec_push(b->ast_refs, (AstGlobal) {
                .is_public = !!is_public,
                .ref = def,
            });
            return 0;
        }
        print_diagnostic(&loc, &Diagnostic(ErrorMultipleDefinition, {0}));
        if (prev_sym.kind == SYM_PRIVATE_GLOBAL || prev_sym.kind == SYM_PUBLIC_GLOBAL) {
            AstRef prev_ref = nth(b->ast_refs->table, prev_sym.global).ref;
            SourceLoc prev_loc = get_ast_location(b, prev_ref);
            print_diagnostic(&prev_loc, &Diagnostic(NotePreviousDefinition, {0}));
        } else {
            print_diagnostic(&loc, &Diagnostic(NotePreviousBuiltinDefinition, {0}));
        }
        return 1;
    }

    int32_t entry = b->ast_refs->len;
    vec_push(b->ast_refs, (AstGlobal) {
        .is_public = !!is_public,
        .ref = def,
    });
    htable_try_insert(scope, name, entry ^ is_public);
    if (is_extern) {
        htable_try_insert(b->extern_symbols, name, entry);
    }
    return 0;
}

#include "internal.h"

typedef struct {
    int32_t file_count;
    Paths paths;
    Sources sources;
    Asts asts;
    Arena scratch;
} ParseStageInfo;

static int parse_all(ParseStageInfo *info) {
    ParseErrorList *parse_errors = arena_alloc(&info->scratch, ParseErrorList, info->file_count);

    int err = 0;

    #pragma omp parallel for reduction (||:err)
    for (int32_t i = 0; i < info->file_count; i++) {
        FileId file = {i};
        ParseInfo p = {
            .path = nth(info->paths, file),
            .source = nth(info->sources, file),
            .ast = &nth(info->asts, file),
            .errors = &parse_errors[i],
        };
        if (!p.source.len || parse_ast(&p)) {
            err = 1;
        }
    }

    for (int32_t i = 0; i < info->file_count; i++) {
        FileId file = {i};
        for (int32_t j = 0; j < parse_errors[i].len; i++) {
            ParseError *e = &parse_errors[i].ptr[j];
            SourceLoc s = {
                .path = nth(info->paths, file),
                .source = nth(info->sources, file),
                .where = e->start,
                .len = e->end.index - e->start.index,
                .mark = e->start,
            };
            print_diagnostic(&s, &e->diag);
        }
        free(parse_errors[i].ptr);
    }

    return err;
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

            if (equals(key, Str("backend"))) {
                options.backend = parse_backend(value);
                continue;
            }

            fprintf(stderr, "ignored unknown argument ");
            fwrite(key.ptr, 1, key.len, stderr);
            fprintf(stderr, "\n");
        } else {
            String option = substring(arg, 1, arg.len);

            if (equals(option, Str("help"))) {
                print_help();
                continue;
            }

            if (equals(option, Str("print-debug"))) {
                options.print_debug = true;
                continue;
            }

            fprintf(stderr, "ignored unknown option ");
            fwrite(option.ptr, 1, option.len, stderr);
            fprintf(stderr, "\n");
        }
    }

    Arena permanent_arena = new_arena(64 << 20);
    Arena scratch_arena = new_arena(64 << 20);

    // Source File Paths

    int32_t file_count = argc - o + 1;
    Paths paths = {arena_alloc(&permanent_arena, char *, file_count)};
    nth(paths, internal_file_id) = "internal.jel";

    for (int32_t i = 1; i < file_count; i++) {
        nth(paths, (FileId) {i}) = argv[o + i - 1];
    }

    // Source Files

    Sources sources = {arena_alloc(&permanent_arena, String, file_count)};
    nth(sources, internal_file_id) = (String) {
        internal_jel_len,
        (char *) internal_jel,
    };

    for (int32_t i = 1; i < file_count; i++) {
        FileId file = {i};
        String source = read_file(nth(paths, file));
        nth(sources, file) = source;
        if (!source.len) {
            fprintf(stderr, "failed to read file \"%s\"\n", nth(paths, file));
        }
    }

    if (options.print_debug) {
        for (int32_t i = 0; i < file_count; i++) {
            FileId file = {i};
            String source = nth(sources, file);
            if (source.len) {
                print_tokens(nth(paths, file), source);
            }
        }
    }

    // Parsing

    if (init_lex_module()) {
        abort();
    }

    Asts asts = {arena_alloc(&permanent_arena, Ast, file_count)};
    int err = parse_all(&(ParseStageInfo) {
        .file_count = file_count,
        .paths = paths,
        .sources = sources,
        .asts = asts,
        .scratch = scratch_arena,
    });

    if (err) {
        return -1;
    }

    if (options.print_debug) {
        for (int32_t i = 0; i < file_count; i++) {
            FileId file = {i};
            print_ast(nth(paths, file), nth(sources, file), &nth(asts, file));
        }
    }

    // Files & Modules

    Files files = {arena_alloc(&permanent_arena, File, file_count)};
    HashTable module_table = htable_init();
    for (int32_t i = 0; i < file_count; i++) {
        FileId file = {i};
        SourceIndex module_token = get_ast_token(&nth(asts, file), null_ast);
        String module_name = id_token_to_string(nth(sources, file), module_token);
        int32_t new_module = module_table.count;
        int64_t module = htable_try_insert(&module_table, module_name, new_module);
        if (module < 0) {
            module = new_module;
        }
        nth(files, file).module = (ModuleId) {module};
        nth(files, file).scope = htable_init();
    }

    Modules modules = {arena_alloc(&permanent_arena, Module, module_table.count)};
    for (int32_t i = 0; i < module_table.count; i++) {
        ModuleId m = {i};
        nth(modules, m).scope = htable_init();
    }

    HashTable global_scope = htable_init();
    #define TYPE(type) htable_try_insert(&global_scope, Str(#type), RESERVED_##type);
    #include "simple-types"
    htable_try_insert(&global_scope, Str("`Size"), RESERVED_SIZE);
    htable_try_insert(&global_scope, Str("`Alignment"), RESERVED_ALIGNMENT);
    htable_try_insert(&global_scope, Str("`align_of"), RESERVED_ALIGNOF);
    htable_try_insert(&global_scope, Str("`size_of"), RESERVED_SIZEOF);
    htable_try_insert(&global_scope, Str("`cast"), RESERVED_CAST);
    htable_try_insert(&global_scope, Str("`zero_extend"), RESERVED_ZERO_EXTEND);
    htable_try_insert(&global_scope, Str("`slice"), RESERVED_SLICE);
    htable_try_insert(&global_scope, Str("`Affine"), RESERVED_AFFINE);
    htable_try_insert(&global_scope, Str("`ArrayLength"), RESERVED_ARRAY_LENGTH_TYPE);

    AstRefVec ast_refs = {0};
    int32_t function_body_count = 0;
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
        b.function_body_count = &function_body_count;
        for (int32_t i = 0; i < file_count; i++) {
            FileId file = {i};
            AstRoot root = ast_get_root(&nth(asts, file), null_ast);
            for (int32_t j = 0; j < root.defs.len; j++) {
                add_global(&b, (AstRef) {root.defs.ptr[j], file});
            }
        }
        htable_free(&extern_symbols);
    }

    TirInput tir_input = {
        .options = &options,
        .file_count = file_count,
        .paths = paths,
        .sources = sources,
        .asts = asts,
        .files = files,
        .module_table = &module_table,
        .modules = modules,
        .global_scope = &global_scope,
        .ast_refs = ast_refs.table,
        .def_count = ast_refs.len,
        .function_body_count = function_body_count,
    };
    TirOutput tir_output = analyze_types(&tir_input, &permanent_arena, scratch_arena);
    if (tir_output.error) {
        return -1;
    }
    if (options.print_debug) {
        for (int32_t i = 0; i < tir_output.global_deps.functions.len; i++) {
            TirContext ctx = {
                .global = &tir_output.global_deps,
                .thread = &tir_output.insts[i].deps,
            };
            TirFunction t = tir_get_function(ctx, tir_output.global_deps.functions.ptr[i]);
            print_tir(
                ctx,
                tir_get_str(ctx, t.name),
                tir_output.insts[i].body_first,
                tir_output.insts[i].body_length
            );
        }
    }
    err = check_substructural_types(
        &(SubstructuralAnalysisInput) {
            .paths = paths,
            .sources = sources,
            .asts = asts,
            .ast_refs = ast_refs.table,
            .global_deps = &tir_output.global_deps,
            .insts = tir_output.insts,
            .functions = tir_output.functions,
            .function_count = function_body_count,
        },
        scratch_arena
    );
    if (err) {
        return -1;
    }

    Mir mir_result = tir_to_mir(&(MirAnalysisInput) {
        .target = options.target,
        .paths = paths,
        .sources = sources,
        .asts = asts,
        .ast_refs = ast_refs.table,
        .functions = tir_output.global_deps.functions.ptr,
        .global_tir = &tir_output.global_deps,
        .function_tirs = tir_output.insts,
        .function_count = tir_output.global_deps.functions.len,
    }, &permanent_arena, scratch_arena);
    GenInput gen_input = {
        .mir = &mir_result,
    };
    switch (options.backend) {
        case BACKEND_C: {
            gen_c(&gen_input, options.target);
            break;
        }
        case BACKEND_LLVM: {
            gen_llvm(&gen_input, options.target);
            break;
        }
    }
}
