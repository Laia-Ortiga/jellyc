#include "print.h"

#include "adt.h"
#include "ast.h"
#include "tir.h"
#include "util.h"

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}



typedef struct {
    String source;
    Ast *ast;
    int depth;
} AstPrinter;

#include "ast-print.c"

void print_ast(char const *path, String source, Ast *ast) {
    AstPrinter printer = {0};
    printer.source = source;
    printer.ast = ast;
    printf("%s: ", path);
    AstRoot t = ast_get_root(ast, null_ast);
    printer.depth++;
    printf("Root(\n");
    print_indent(printer.depth);
    printf("token: ");
    Lexer lexer = new_lexer(substring(source, t.token.index, source.len));
    Token token = next_token(&lexer);
    String s = substring(lexer.source, token.start.index, token.end.index);
    fwrite(s.ptr, 1, s.len, stdout);
    printf(",\n");
    print_indent(printer.depth++);
    printf("defs: [\n");
    for (int32_t i = 0; i < t.defs.len; i++) {
        print_indent(printer.depth);
        print_ast_node(&printer, t.defs.ptr[i]);
        printf(",\n");
    }
    print_indent(--printer.depth);
    printf("],\n");
    print_indent(--printer.depth);
    printf(")\n");
}

void print_node(String source, Ast *ast, AstId node) {
    AstPrinter printer = {0};
    printer.source = source;
    printer.ast = ast;
    print_ast_node(&printer, node);
    printf("\n");
}



typedef struct {
    TirContext tir;
    int depth;
} TirPrinter;

#include "tir-print.c"

void print_tir(
    TirContext c,
    char const *name,
    int32_t first,
    int32_t length
) {
    TirPrinter printer = {0};
    printer.tir = c;
    printf("%s: Tir(\n", name);
    printer.depth++;
    for (int32_t i = 0; i < length; i++) {
        print_indent(printer.depth);
        TirId statement = {get_term_extra(c.thread, first + i)};
        print_tir_node(&printer, statement);
        printf(",\n");
    }
    printf(")\n");
}

void print_tir_term(TirContext c, TirId term) {
    TirPrinter printer = {0};
    printer.tir = c;
    print_tir_node(&printer, term);
    printf("\n");
}

void print_type(FILE *file, TirContext c, TirId type) {
    switch (get_tir_tag(c, type)) {
        case TIR_ERROR: {
            fprintf(file, "{error}");
            return;
        }
        case TIR_RESERVED: {
            switch (tir_as_reserved(type)) {
                case RESERVED_VOID: fprintf(file, "void"); return;

                #define TYPE(type) case RESERVED_##type: fprintf(file, #type); return;
                #include "simple-types"

                default: break;
            }
            compiler_error("print_type: unknown primitive type");
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType array = tir_get_array_type(c, type);
            int64_t length = tir_get_array_length_type(c, array.index).length;
            fprintf(file, "[:%ld]", length);
            print_type(file, c, array.elem);
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            int64_t length = tir_get_array_length_type(c, type).length;
            fprintf(file, "`ArrayLength(%ld)", length);
            return;
        }
        case TIR_PTR_TYPE: {
            fprintf(file, "*");
            print_type(file, c, tir_get_ptr_type(c, type).elem);
            return;
        }
        case TIR_MUT_PTR_TYPE: {
            fprintf(file, "*mut ");
            print_type(file, c, tir_get_ptr_type(c, type).elem);
            return;
        }
        case TIR_SLICE_TYPE: {
            fprintf(file, "@");
            print_type(file, c, tir_get_slice_type(c, type).elem);
            return;
        }
        case TIR_MUT_SLICE_TYPE: {
            fprintf(file, "@mut ");
            print_type(file, c, tir_get_slice_type(c, type).elem);
            return;
        }
        case TIR_FUNCTION_TYPE: {
            TirFunctionType f = tir_get_function_type(c, type);
            fprintf(file, "function (");
            for (int32_t j = 0; j < f.params.len; j++) {
                if (j != 0) {
                    fprintf(file, ", ");
                }
                print_type(file, c, f.params.ptr[j]);
            }
            fprintf(file, ")");
            if (!tir_is_reserved(f.ret, RESERVED_VOID)) {
                fprintf(file, " -> ");
                print_type(file, c, f.ret);
            }
            return;
        }
        case TIR_ENUM_TYPE: {
            fprintf(file, "%s", tir_get_str(c, tir_get_enum_type(c, type).name));
            return;
        }
        case TIR_TAGGED_TYPE: {
            TirTaggedType t = tir_get_tagged_type(c, type);
            fprintf(file, "%s", tir_get_str(c, t.name));

            if (t.args.len) {
                fprintf(file, "[");
                for (int32_t j = 0; j < t.args.len; j++) {
                    if (j != 0) {
                        fprintf(file, ", ");
                    }
                    print_type(file, c, t.args.ptr[j]);
                }
                fprintf(file, "]");
            }
            return;
        }
        case TIR_AFFINE_TYPE: {
            fprintf(file, "`Affine[");
            print_type(file, c, tir_get_affine_type(c, type).elem);
            fprintf(file, "]");
            return;
        }
        case TIR_TYPE_PARAMETER: {
            TirTypeParameter t = tir_get_type_parameter(c, type);
            fputs(tir_get_str(c, t.name), file);
            return;
        }
        default: {
            compiler_error("print_type: unknown type tag");
        }
    }
}

void debug_type(TirContext c, TirId type) {
    print_type(stderr, c, type);
    printf("\n");
}
