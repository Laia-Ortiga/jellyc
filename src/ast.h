#pragma once

#include "lex.h"
#include "ids.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    AST_ROOT,

    // Global definitions

    AST_IMPORT,
    AST_PUBLIC,
    AST_FUNCTION,
    AST_ENUM,
    AST_STRUCT,
    AST_NEWTYPE,
    AST_CONST,
    AST_EXTERN_FUNCTION,
    AST_EXTERN_MUT,

    // Local definitions

    AST_PARAM,
    AST_LET,
    AST_MUT,

    // Statements

    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_SWITCH,
    AST_SWITCH_CASE,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,

    // Types

    AST_ARRAY_TYPE,
    AST_ARRAY_TYPE_SUGAR,
    AST_POINTER_TYPE,
    AST_POINTER_MUT_TYPE,
    AST_SLICE_TYPE,
    AST_SLICE_MUT_TYPE,
    AST_FUNCTION_TYPE,

    // Unary operators

    AST_PLUS,
    AST_MINUS,
    AST_NOT,
    AST_ADDRESS,
    AST_DEREF,

    // Binary operators

    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,

    AST_AND,
    AST_OR,
    AST_XOR,
    AST_SHL,
    AST_SHR,

    AST_LOGIC_AND,
    AST_LOGIC_OR,

    AST_EQ,
    AST_NE,
    AST_LT,
    AST_GT,
    AST_LE,
    AST_GE,

    AST_ASSIGN,
    AST_ASSIGN_ADD,
    AST_ASSIGN_SUB,
    AST_ASSIGN_MUL,
    AST_ASSIGN_DIV,
    AST_ASSIGN_MOD,
    AST_ASSIGN_AND,
    AST_ASSIGN_OR,
    AST_ASSIGN_XOR,

    AST_TYPE_HINT,

    // Miscellaneous

    AST_CALL,
    AST_INDEX,
    AST_SLICE,
    AST_ACCESS,
    AST_INFERRED_ACCESS,
    AST_LIST,
    AST_BLOCK,

    // Tree leaves

    AST_ID,
    AST_INT,
    AST_FLOAT,
    AST_CHAR,
    AST_STRING,
    AST_BOOL,
    AST_NULL,
} AstTag;

typedef struct {
    SourceIndex token;
    int32_t left;
    int32_t right;
} AstData;

typedef struct {
    int32_t private_field_id;
} AstId;

typedef struct {
    SumVecTable(AstId, AstData) nodes;
    Vec(int32_t) extra;
} Ast;

static AstId const null_ast = {0};

static inline bool is_ast_null(AstId ast_id) {
    return !ast_id.private_field_id;
}

static inline AstTag get_ast_tag(AstId node, Ast const *ast) {
    return nth(ast->nodes.tag_table, node);
}

static inline SourceIndex get_ast_token(AstId node, Ast const *ast) {
    return nth(ast->nodes.data_table, node).token;
}

typedef struct {
    AstId left;
    AstId right;
} AstBinary;

typedef struct {
    int32_t count;
    AstId *nodes;
} AstList;

typedef struct {
    int32_t type_param_count;
    AstId const *type_params;
    int32_t param_count;
    AstId const *params;
    AstId ret;
    AstId body;
} AstFunction;

typedef struct {
    int32_t type_param_count;
    AstId const *type_params;
    int32_t field_count;
    AstId const *fields;
} AstStruct;

typedef struct {
    AstId repr;
    int32_t member_count;
    AstId const *members;
} AstEnum;

typedef struct {
    int32_t type_param_count;
    AstId const *type_params;
    AstId type;
} AstNewtype;

typedef struct {
    AstId operand;
    int32_t arg_count;
    AstId const *args;
} AstCall;

typedef struct {
    AstId condition;
    AstId true_block;
    AstId false_block;
} AstIf;

typedef struct {
    AstId init;
    AstId condition;
    AstId next;
    AstId block;
} AstFor;

static inline AstList get_ast_list(AstId node, Ast const *ast) {
    int32_t count = nth(ast->nodes.data_table, node).left;
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstList) {count, (AstId *) &ast->extra.ptr[extra]};
}

static inline AstId get_ast_unary(AstId node, Ast const *ast) {
    return (AstId) {nth(ast->nodes.data_table, node).left};
}

static inline AstBinary get_ast_binary(AstId node, Ast const *ast) {
    return (AstBinary) {
        {nth(ast->nodes.data_table, node).left},
        {nth(ast->nodes.data_table, node).right},
    };
}

static inline int64_t get_ast_int(AstId node, Ast const *ast) {
    return load_i64(
        nth(ast->nodes.data_table, node).left,
        nth(ast->nodes.data_table, node).right
    );
}

static inline double get_ast_float(AstId node, Ast const *ast) {
    return load_f64(
        nth(ast->nodes.data_table, node).left,
        nth(ast->nodes.data_table, node).right
    );
}

static inline AstFunction get_ast_function(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstFunction) {
        .type_param_count = ast->extra.ptr[extra],
        .type_params = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 1]],
        .param_count = ast->extra.ptr[extra + 2],
        .params = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 3]],
        .ret = {ast->extra.ptr[extra + 4]},
        .body = {nth(ast->nodes.data_table, node).left},
    };
}

static inline AstFunction get_ast_extern_function(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstFunction) {
        .param_count = nth(ast->nodes.data_table, node).left,
        .params = (AstId const *) &ast->extra.ptr[extra + 1],
        .ret = {ast->extra.ptr[extra]},
    };
}

static inline AstStruct get_ast_struct(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstStruct) {
        .type_param_count = ast->extra.ptr[extra],
        .type_params = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 1]],
        .field_count = nth(ast->nodes.data_table, node).left,
        .fields = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 2]],
    };
}

static inline AstEnum get_ast_enum(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstEnum) {
        .repr = {nth(ast->nodes.data_table, node).left},
        .member_count = ast->extra.ptr[extra],
        .members = (AstId const *) &ast->extra.ptr[extra + 1],
    };
}

static inline AstNewtype get_ast_newtype(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstNewtype) {
        .type_param_count = ast->extra.ptr[extra],
        .type_params = (AstId const *) &ast->extra.ptr[extra + 1],
        .type = {nth(ast->nodes.data_table, node).left},
    };
}

static inline AstCall get_ast_call(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstCall) {
        .operand = {ast->extra.ptr[extra]},
        .arg_count = nth(ast->nodes.data_table, node).left,
        .args = (AstId const *) &ast->extra.ptr[extra + 1],
    };
}

static inline AstIf get_ast_if(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstIf) {
        .condition = {nth(ast->nodes.data_table, node).left},
        .true_block = {ast->extra.ptr[extra]},
        .false_block = {ast->extra.ptr[extra + 1]},
    };
}

static inline AstFor get_ast_for(AstId node, Ast const *ast) {
    int32_t extra = nth(ast->nodes.data_table, node).right;
    return (AstFor) {
        .init = {ast->extra.ptr[extra]},
        .condition = {ast->extra.ptr[extra + 1]},
        .next = {ast->extra.ptr[extra + 2]},
        .block = {nth(ast->nodes.data_table, node).left},
    };
}
