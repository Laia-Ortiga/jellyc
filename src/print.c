#include "print.h"

#include "adt.h"
#include "ast.h"
#include "tir.h"
#include "lex.h"

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("    ");
    }
}



typedef struct {
    String source;
    Ast const *ast;
    int depth;
} AstPrinter;

static void print_ast_node(AstPrinter *printer, AstId node);

static void print_ast_list(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    AstList list = get_ast_list(node, printer->ast);
    for (int32_t i = 0; i < list.count; i++) {
        print_ast_node(printer, list.nodes[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_if(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("If(\n");
    printer->depth++;
    AstIf if_ = get_ast_if(node, printer->ast);
    print_ast_node(printer, if_.condition);
    print_ast_node(printer, if_.true_block);
    print_ast_node(printer, if_.false_block);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_for(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("For(\n");
    printer->depth++;
    AstFor for_ = get_ast_for(node, printer->ast);
    print_ast_node(printer, for_.init);
    print_ast_node(printer, for_.condition);
    print_ast_node(printer, for_.next);
    print_ast_node(printer, for_.block);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_unary(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_ast_node(printer, get_ast_unary(node, printer->ast));
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_binary(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    AstBinary bin = get_ast_binary(node, printer->ast);
    print_ast_node(printer, bin.left);
    print_ast_node(printer, bin.right);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_function(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("Function(\n");
    printer->depth++;
    AstFunction f = get_ast_function(node, printer->ast);

    print_indent(printer->depth);
    printf("TypeParameters(\n");
    printer->depth++;
    for (int32_t i = 0; i < f.type_param_count; i++) {
        print_ast_node(printer, f.type_params[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");

    print_indent(printer->depth);
    printf("Parameters(\n");
    printer->depth++;
    for (int32_t i = 0; i < f.param_count; i++) {
        print_ast_node(printer, f.params[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");

    print_ast_node(printer, f.ret);
    print_ast_node(printer, f.body);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_newtype(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("Newtype(\n");
    printer->depth++;
    AstNewtype n = get_ast_newtype(node, printer->ast);

    print_indent(printer->depth);
    printf("TypeParameters(\n");
    printer->depth++;
    for (int32_t i = 0; i < n.type_param_count; i++) {
        print_ast_node(printer, n.type_params[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");

    print_ast_node(printer, n.type);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_extern_function(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    AstFunction f = get_ast_extern_function(node, printer->ast);

    print_indent(printer->depth);
    printf("Parameters(\n");
    printer->depth++;
    for (int32_t i = 0; i < f.param_count; i++) {
        print_ast_node(printer, f.params[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");

    print_ast_node(printer, f.ret);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_function_type(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("FunctionType(\n");
    printer->depth++;
    AstCall f = get_ast_call(node, printer->ast);

    print_indent(printer->depth);
    printf("Parameters(\n");
    printer->depth++;
    for (int32_t i = 0; i < f.arg_count; i++) {
        print_ast_node(printer, f.args[i]);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");

    print_ast_node(printer, f.operand);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_call(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    AstCall call = get_ast_call(node, printer->ast);
    print_ast_node(printer, call.operand);

    for (int32_t i = 0; i < call.arg_count; i++) {
        print_ast_node(printer, call.args[i]);
    }

    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_leaf(AstPrinter *printer, char const *name) {
    print_indent(printer->depth);
    printf("%s\n", name);
}

static void print_ast_id_token(AstPrinter *printer, AstId node) {
    SourceIndex token = get_ast_token(node, printer->ast);
    String s = id_token_to_string(printer->source, token);
    fwrite(s.ptr, 1, s.len, stdout);
}

static void print_ast_id(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("Id(");
    print_ast_id_token(printer, node);
    printf(")\n");
}

static void print_ast_param(AstPrinter *printer, char const *name, AstId node) {
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_indent(printer->depth);
    print_ast_id_token(printer, node);
    printf("\n");
    print_ast_node(printer, get_ast_unary(node, printer->ast));
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_ast_int(AstPrinter *printer, AstId node) {
    print_indent(printer->depth);
    printf("Int(%ld)\n", get_ast_int(node, printer->ast));
}

static void print_ast_node(AstPrinter *printer, AstId node) {
    switch (get_ast_tag(node, printer->ast)) {
        case AST_ROOT: print_ast_leaf(printer, "Null"); break;
        case AST_IMPORT: print_ast_leaf(printer, "Import"); break;
        case AST_PUBLIC: print_ast_unary(printer, "Public", node); break;
        case AST_FUNCTION: print_ast_function(printer, node); break;
        case AST_STRUCT: print_ast_list(printer, "Struct", node); break;
        case AST_ENUM: print_ast_call(printer, "Enum", node); break;
        case AST_NEWTYPE: print_ast_newtype(printer, node); break;
        case AST_EXTERN_FUNCTION: print_ast_extern_function(printer, "ExternFunction", node); break;
        case AST_EXTERN_MUT: print_ast_unary(printer, "ExternMut", node); break;
        case AST_CONST: print_ast_unary(printer, "Const", node); break;
        case AST_PARAM: print_ast_param(printer, "Param", node); break;
        case AST_SWITCH_CASE: print_ast_binary(printer, "Case", node); break;
        case AST_ARRAY_TYPE: print_ast_binary(printer, "ArrayType", node); break;
        case AST_ARRAY_TYPE_SUGAR: print_ast_binary(printer, "ArrayTypeSugar", node); break;
        case AST_FUNCTION_TYPE: print_ast_function_type(printer, node); break;
        case AST_ID: print_ast_id(printer, node); break;
        case AST_INT: print_ast_int(printer, node); break;
        case AST_FLOAT: print_ast_leaf(printer, "Float"); break;
        case AST_CHAR: print_ast_leaf(printer, "Char"); break;
        case AST_STRING: print_ast_leaf(printer, "String"); break;
        case AST_BOOL: print_ast_leaf(printer, "Bool"); break;
        case AST_NULL: print_ast_leaf(printer, "Null"); break;
        case AST_PLUS: print_ast_unary(printer, "Plus", node); break;
        case AST_MINUS: print_ast_unary(printer, "Minus", node); break;
        case AST_NOT: print_ast_unary(printer, "Not", node); break;
        case AST_ADDRESS: print_ast_unary(printer, "Address", node); break;
        case AST_DEREF: print_ast_unary(printer, "Deref", node); break;
        case AST_POINTER_TYPE: print_ast_unary(printer, "*", node); break;
        case AST_POINTER_MUT_TYPE: print_ast_unary(printer, "*mut", node); break;
        case AST_SLICE_TYPE: print_ast_unary(printer, "@", node); break;
        case AST_SLICE_MUT_TYPE: print_ast_unary(printer, "@mut", node); break;
        case AST_ADD: print_ast_binary(printer, "Add", node); break;
        case AST_SUB: print_ast_binary(printer, "Sub", node); break;
        case AST_MUL: print_ast_binary(printer, "Mul", node); break;
        case AST_DIV: print_ast_binary(printer, "Div", node); break;
        case AST_MOD: print_ast_binary(printer, "Mod", node); break;
        case AST_AND: print_ast_binary(printer, "And", node); break;
        case AST_OR: print_ast_binary(printer, "Or", node); break;
        case AST_XOR: print_ast_binary(printer, "Xor", node); break;
        case AST_SHL: print_ast_binary(printer, "Shl", node); break;
        case AST_SHR: print_ast_binary(printer, "Shr", node); break;
        case AST_LOGIC_AND: print_ast_binary(printer, "LogicAnd", node); break;
        case AST_LOGIC_OR: print_ast_binary(printer, "LogicOr", node); break;
        case AST_EQ: print_ast_binary(printer, "==", node); break;
        case AST_NE: print_ast_binary(printer, "!=", node); break;
        case AST_LT: print_ast_binary(printer, "<", node); break;
        case AST_GT: print_ast_binary(printer, ">", node); break;
        case AST_LE: print_ast_binary(printer, "<=", node); break;
        case AST_GE: print_ast_binary(printer, ">=", node); break;
        case AST_ASSIGN: print_ast_binary(printer, "=", node); break;
        case AST_ASSIGN_ADD: print_ast_binary(printer, "+=", node); break;
        case AST_ASSIGN_SUB: print_ast_binary(printer, "-=", node); break;
        case AST_ASSIGN_MUL: print_ast_binary(printer, "*=", node); break;
        case AST_ASSIGN_DIV: print_ast_binary(printer, "/=", node); break;
        case AST_ASSIGN_MOD: print_ast_binary(printer, "%=", node); break;
        case AST_ASSIGN_AND: print_ast_binary(printer, "&=", node); break;
        case AST_ASSIGN_OR: print_ast_binary(printer, "|=", node); break;
        case AST_ASSIGN_XOR: print_ast_binary(printer, "^=", node); break;
        case AST_ACCESS: print_ast_unary(printer, "Access", node); break;
        case AST_INFERRED_ACCESS: print_ast_leaf(printer, "InferredAccess"); break;
        case AST_TYPE_HINT: print_ast_binary(printer, "TypeHint", node); break;
        case AST_LET: print_ast_param(printer, "Let", node); break;
        case AST_MUT: print_ast_param(printer, "Mut", node); break;
        case AST_CALL: print_ast_call(printer, "Call", node); break;
        case AST_INDEX: print_ast_call(printer, "Index", node); break;
        case AST_SLICE: print_ast_call(printer, "Slice", node); break;
        case AST_MAP: print_ast_list(printer, "Map", node); break;
        case AST_MAP_ENTRY: print_ast_param(printer, "MapEntry", node); break;
        case AST_LIST: print_ast_list(printer, "List", node); break;
        case AST_BLOCK: print_ast_list(printer, "Block", node); break;
        case AST_IF: print_ast_if(printer, node); break;
        case AST_WHILE: print_ast_binary(printer, "While", node); break;
        case AST_FOR: print_ast_for(printer, node); break;
        case AST_SWITCH: print_ast_call(printer, "Switch", node); break;
        case AST_BREAK: print_ast_leaf(printer, "Break"); break;
        case AST_CONTINUE: print_ast_leaf(printer, "Continue"); break;
        case AST_RETURN: print_ast_unary(printer, "Return", node); break;
    }
}

void print_ast(char const *path, String source, Ast const *ast) {
    AstPrinter printer = {0};
    printer.source = source;
    printer.ast = ast;
    printf("Ast(%s) {\n", path);
    printer.depth++;
    AstList list = get_ast_list(null_ast, ast);
    for (int32_t i = 0; i < list.count; i++) {
        print_ast_node(&printer, list.nodes[i]);
    }
    printf("}\n");
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
    printf("Tir(%s) {\n", name);
    printer.depth++;
    for (int32_t i = 0; i < length; i++) {
        TirId statement = {get_term_extra(c.thread, first + i)};
        print_tir_node(&printer, statement);
    }
    printf("}\n");
}

void print_tir_term(TirContext c, TirId term) {
    TirPrinter printer = {0};
    printer.tir = c;
    print_tir_node(&printer, term);
}

void print_type(FILE *file, TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_RESERVED: {
            switch ((ReservedTerm) type.id) {
                case RESERVED_ERROR: fprintf(file, "{error}"); return;
                case TYPE_VOID: fprintf(file, "void"); return;

                #define TYPE(type) case TYPE_##type: fprintf(file, #type); return;
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
            if (f.ret.id != TYPE_VOID) {
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
