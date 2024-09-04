#include "print.h"

#include "adt.h"
#include "fwd.h"
#include "data/ast.h"
#include "data/tir.h"
#include "lex.h"

#include <stdlib.h>

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
    printf("%d\n", n.count);
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
        case AST_DEREF: print_ast_unary(printer, "*", node); break;
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
        case AST_CAST: print_ast_binary(printer, "Cast", node); break;
        case AST_LET: print_ast_param(printer, "Let", node); break;
        case AST_MUT: print_ast_param(printer, "Mut", node); break;
        case AST_CALL: print_ast_call(printer, "Call", node); break;
        case AST_INDEX: print_ast_call(printer, "Index", node); break;
        case AST_SLICE: print_ast_call(printer, "Slice", node); break;
        case AST_LIST: print_ast_list(printer, "List", node); break;
        case AST_BLOCK: print_ast_list(printer, "Block", node); break;
        case AST_EXPRESSION_STATEMENT: print_ast_node(printer, get_ast_unary(node, printer->ast)); break;
        case AST_IF: print_ast_if(printer, node); break;
        case AST_WHILE: print_ast_binary(printer, "While", node); break;
        case AST_FOR_HELPER: break;
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
    TirContext context;
    TirInstList *tir;
    int depth;
} TirPrinter;

static void print_tir_node(TirPrinter *printer, TirId tir_id, TypeId type);

static void print_tir_value(TirPrinter *printer, ValueId value) {
    switch (get_value_tag(printer->context, value)) {
        case VAL_ERROR: {
            print_indent(printer->depth);
            printf("(error)\n");
            break;
        }
        case VAL_FUNCTION:
        case VAL_EXTERN_FUNCTION:
        case VAL_EXTERN_VAR: {
            print_indent(printer->depth);
            int32_t s = get_value_data(printer->context, value)->index;
            printf("%s\n", &printer->context.global->strtab.ptr[s]);
            break;
        }
        case VAL_CONST_INT: {
            print_indent(printer->depth);
            printf("%ld\n", get_value_int(printer->context, value));
            break;
        }
        case VAL_CONST_FLOAT: {
            print_indent(printer->depth);
            printf("%f\n", get_value_float(printer->context, value));
            break;
        }
        case VAL_CONST_NULL: {
            print_indent(printer->depth);
            printf("null\n");
            break;
        }
        case VAL_STRING: {
            print_indent(printer->depth);
            printf("\"%s\"\n", get_value_str(printer->context, value));
            break;
        }
        case VAL_VARIABLE:
        case VAL_MUTABLE_VARIABLE: {
            print_indent(printer->depth);
            printf("variable_%d: ", get_value_data(printer->context, value)->index);
            print_type(stdout, printer->context, get_value_data(printer->context, value)->type);
            printf("\n");
            break;
        }
        case VAL_TEMPORARY: {
            TirId tir_id = {get_value_data(printer->context, value)->index};
            TypeId type = get_value_data(printer->context, value)->type;
            print_tir_node(printer, tir_id, type);
            break;
        }
    }
}

static void print_tir_value_ref(TirPrinter *printer, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId value = {data.left};
    print_tir_value(printer, value);
}

static void print_tir_leaf_statement(TirPrinter *printer, char const *name) {
    print_indent(printer->depth);
    printf("%s\n", name);
}

static void print_tir_let(TirPrinter *printer, char const *name, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    int32_t var_id = data.left;
    ValueId init = {data.right};
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_indent(printer->depth);
    printf("variable_%d\n", var_id);
    print_tir_value(printer, init);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_unary_value(TirPrinter *printer, char const *name, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId operand = {data.left};
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_tir_value(printer, operand);
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_binary_value(TirPrinter *printer, char const *name, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId left = {data.left};
    ValueId right = {data.right};
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_tir_value(printer, left);
    print_tir_value(printer, right);
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_return(TirPrinter *printer, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId operand = {data.left};
    print_indent(printer->depth);
    printf("return(\n");
    printer->depth++;
    if (operand.id) {
        print_tir_value(printer, operand);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_access(TirPrinter *printer, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId operand = {data.left};
    int32_t index = data.right;
    print_indent(printer->depth);
    printf("access(\n");
    printer->depth++;
    print_tir_value(printer, operand);
    print_indent(printer->depth);
    printf("%d\n", index);
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_call(TirPrinter *printer, char const *name, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId operand = {data.left};
    int32_t args = data.right;
    TypeId function_type = get_value_type(printer->context, operand);
    int32_t arg_count = get_function_type(printer->context, function_type).param_count;
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_tir_value(printer, operand);
    for (int32_t i = 0; i < arg_count; i++) {
        ValueId arg = {get_tir_extra(printer->tir, args + i)};
        print_tir_value(printer, arg);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")");
    if (type.id != TYPE_VOID) {
        printf(": ");
        print_type(stdout, printer->context, type);
    }
    printf("\n");
}

static void print_tir_slice(TirPrinter *printer, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId operand = {data.left};
    int32_t index = data.right;
    ValueId low = {get_tir_extra(printer->tir, index)};
    ValueId high = {get_tir_extra(printer->tir, index + 1)};
    print_indent(printer->depth);
    printf("slice(\n");
    printer->depth++;
    print_tir_value(printer, operand);
    print_tir_value(printer, low);
    print_tir_value(printer, high);
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_new_type(TirPrinter *printer, char const *name, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    int32_t args = data.left;
    int32_t arg_count = data.right;
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    for (int32_t i = 0; i < arg_count; i++) {
        ValueId arg = {get_tir_extra(printer->tir, args + i)};
        print_tir_value(printer, arg);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_assignment(TirPrinter *printer, char const *name, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId left = {data.left};
    ValueId right = {data.right};
    print_indent(printer->depth);
    printf("%s(\n", name);
    printer->depth++;
    print_tir_value(printer, left);
    print_tir_value(printer, right);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_switch(TirPrinter *printer, TirId tir_id, TypeId type) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId switch_ = {data.left};
    int32_t extra = data.right;
    int32_t branches = get_tir_extra(printer->tir, extra);
    int32_t branch_count = get_tir_extra(printer->tir, extra + 1);
    print_indent(printer->depth);
    printf("switch(\n");
    printer->depth++;
    print_tir_value(printer, switch_);
    for (int32_t i = 0; i < branch_count; i++) {
        print_indent(printer->depth);
        printf("case(\n");
        printer->depth++;
        ValueId pattern = {get_tir_extra(printer->tir, branches + i * 2)};
        ValueId value = {get_tir_extra(printer->tir, branches + i * 2 + 1)};
        if (pattern.id) {
            print_tir_value(printer, pattern);
        } else {
            print_indent(printer->depth);
            printf("else\n");
        }
        print_tir_value(printer, value);
        printer->depth--;
        print_indent(printer->depth);
        printf(")");
        printf("\n");
    }
    printer->depth--;
    print_indent(printer->depth);
    printf("): ");
    print_type(stdout, printer->context, type);
    printf("\n");
}

static void print_tir_block(TirPrinter *printer, int32_t block, int32_t length) {
    print_indent(printer->depth);
    printf("block(\n");
    printer->depth++;
    for (int32_t i = 0; i < length; i++) {
        TirId statement = {get_tir_extra(printer->tir, block + i)};
        print_tir_node(printer, statement, null_type);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_if(TirPrinter *printer, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId condition = {data.left};
    int32_t extra = data.right;
    int32_t true_block = get_tir_extra(printer->tir, extra);
    int32_t true_block_length = get_tir_extra(printer->tir, extra + 1);
    int32_t false_block = get_tir_extra(printer->tir, extra + 2);
    int32_t false_block_length = get_tir_extra(printer->tir, extra + 3);
    print_indent(printer->depth);
    printf("if(\n");
    printer->depth++;
    print_tir_value(printer, condition);
    print_tir_block(printer, true_block, true_block_length);
    print_tir_block(printer, false_block, false_block_length);
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_for(TirPrinter *printer, TirId tir_id) {
    TirInstData data = get_tir_data(printer->tir, tir_id);
    ValueId condition = {data.left};
    int32_t extra = data.right;
    ValueId next = {get_tir_extra(printer->tir, extra)};
    int32_t block = get_tir_extra(printer->tir, extra + 1);
    int32_t block_length = get_tir_extra(printer->tir, extra + 2);
    print_indent(printer->depth);
    printf("for(\n");
    printer->depth++;
    print_tir_value(printer, condition);
    print_tir_block(printer, block, block_length);
    if (next.id) {
        print_tir_value(printer, next);
    }
    printer->depth--;
    print_indent(printer->depth);
    printf(")\n");
}

static void print_tir_node(TirPrinter *printer, TirId tir_id, TypeId type) {
    switch (get_tir_tag(printer->tir, tir_id)) {
        case TIR_FUNCTION: abort();
        case TIR_LET: print_tir_let(printer, "let", tir_id); break;
        case TIR_MUT: print_tir_let(printer, "mut", tir_id); break;
        case TIR_VALUE: print_tir_value_ref(printer, tir_id); break;
        case TIR_PLUS: print_tir_unary_value(printer, "plus", tir_id, type); break;
        case TIR_MINUS: print_tir_unary_value(printer, "minus", tir_id, type); break;
        case TIR_NOT: print_tir_unary_value(printer, "not", tir_id, type); break;
        case TIR_ADDRESS_OF_TEMPORARY: print_tir_unary_value(printer, "address temporary", tir_id, type); break;
        case TIR_ADDRESS: print_tir_unary_value(printer, "address", tir_id, type); break;
        case TIR_DEREF: print_tir_unary_value(printer, "deref", tir_id, type); break;
        case TIR_ADD: print_tir_binary_value(printer, "add", tir_id, type); break;
        case TIR_SUB: print_tir_binary_value(printer, "sub", tir_id, type); break;
        case TIR_MUL: print_tir_binary_value(printer, "mul", tir_id, type); break;
        case TIR_DIV: print_tir_binary_value(printer, "div", tir_id, type); break;
        case TIR_MOD: print_tir_binary_value(printer, "mod", tir_id, type); break;
        case TIR_AND: print_tir_binary_value(printer, "and", tir_id, type); break;
        case TIR_OR: print_tir_binary_value(printer, "or", tir_id, type); break;
        case TIR_XOR: print_tir_binary_value(printer, "xor", tir_id, type); break;
        case TIR_SHL: print_tir_binary_value(printer, "shl", tir_id, type); break;
        case TIR_SHR: print_tir_binary_value(printer, "shr", tir_id, type); break;
        case TIR_EQ: print_tir_binary_value(printer, "eq", tir_id, type); break;
        case TIR_NE: print_tir_binary_value(printer, "ne", tir_id, type); break;
        case TIR_LT: print_tir_binary_value(printer, "lt", tir_id, type); break;
        case TIR_GT: print_tir_binary_value(printer, "gt", tir_id, type); break;
        case TIR_LE: print_tir_binary_value(printer, "le", tir_id, type); break;
        case TIR_GE: print_tir_binary_value(printer, "ge", tir_id, type); break;
        case TIR_ASSIGN: print_tir_assignment(printer, "assign", tir_id); break;
        case TIR_ASSIGN_ADD: print_tir_assignment(printer, "assign add", tir_id); break;
        case TIR_ASSIGN_SUB: print_tir_assignment(printer, "assign sub", tir_id); break;
        case TIR_ASSIGN_MUL: print_tir_assignment(printer, "assign mul", tir_id); break;
        case TIR_ASSIGN_DIV: print_tir_assignment(printer, "assign div", tir_id); break;
        case TIR_ASSIGN_MOD: print_tir_assignment(printer, "assign mod", tir_id); break;
        case TIR_ASSIGN_AND: print_tir_assignment(printer, "assign and", tir_id); break;
        case TIR_ASSIGN_OR: print_tir_assignment(printer, "assign or", tir_id); break;
        case TIR_ASSIGN_XOR: print_tir_assignment(printer, "assign xor", tir_id); break;
        case TIR_ACCESS: print_tir_access(printer, tir_id, type); break;
        case TIR_ITOF: print_tir_unary_value(printer, "cast itof", tir_id, type); break;
        case TIR_ITRUNC: print_tir_unary_value(printer, "cast itrunc", tir_id, type); break;
        case TIR_SEXT: print_tir_unary_value(printer, "cast sext", tir_id, type); break;
        case TIR_ZEXT: print_tir_unary_value(printer, "cast zext", tir_id, type); break;
        case TIR_FTOI: print_tir_unary_value(printer, "cast ftoi", tir_id, type); break;
        case TIR_FTRUNC: print_tir_unary_value(printer, "cast ftrunc", tir_id, type); break;
        case TIR_FEXT: print_tir_unary_value(printer, "cast fext", tir_id, type); break;
        case TIR_PTR_CAST: print_tir_unary_value(printer, "cast ptr", tir_id, type); break;
        case TIR_NOP: print_tir_unary_value(printer, "cast nop", tir_id, type); break;
        case TIR_ARRAY_TO_SLICE: print_tir_unary_value(printer, "cast slice", tir_id, type); break;
        case TIR_CALL: print_tir_call(printer, "call", tir_id, type); break;
        case TIR_INDEX: print_tir_binary_value(printer, "index", tir_id, type); break;
        case TIR_SLICE: print_tir_slice(printer, tir_id, type); break;
        case TIR_NEW_STRUCT: print_tir_new_type(printer, "struct", tir_id, type); break;
        case TIR_NEW_ARRAY: print_tir_new_type(printer, "array", tir_id, type); break;
        case TIR_IF: print_tir_if(printer, tir_id); break;
        case TIR_SWITCH: print_tir_switch(printer, tir_id, type); break;
        case TIR_LOOP: print_tir_for(printer, tir_id); break;
        case TIR_BREAK: print_tir_leaf_statement(printer, "break"); break;
        case TIR_CONTINUE: print_tir_leaf_statement(printer, "continue"); break;
        case TIR_RETURN: print_tir_return(printer, tir_id); break;
    }
}

void print_tir(TirContext context, char const *name, TirInstList *tir, TirId first) {
    TirPrinter printer = {0};
    printer.context = context;
    printer.tir = tir;
    printf("Tir(%s) {\n", name);
    printer.depth++;
    int32_t block = get_tir_data(tir, first).left;
    int32_t block_length = get_tir_data(tir, first).right;
    for (int32_t i = 0; i < block_length; i++) {
        TirId statement = {get_tir_extra(tir, block + i)};
        print_tir_node(&printer, statement, null_type);
    }
    printf("}\n");
}
