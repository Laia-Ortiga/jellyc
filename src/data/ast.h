#pragma once

#include "enums.h"
#include "lex.h"
#include "util.h"
#include "wrappers.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    SourceIndex token;
    int32_t left;
    int32_t right;
} AstData;

typedef struct {
    SumVec(AstData) nodes;
    Vec(int32_t) extra;
} Ast;

static inline AstTag get_ast_tag(AstId node, Ast const *ast) {
    return ast->nodes.tags[node.private_field_id];
}

static inline SourceIndex get_ast_token(AstId node, Ast const *ast) {
    return ast->nodes.datas[node.private_field_id].token;
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
    int32_t field_count;
    AstId const *fields;
} AstStruct;

typedef struct {
    AstId repr;
    int32_t member_count;
    AstId const *members;
} AstEnum;

typedef struct {
    int32_t count;
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
    int32_t count = ast->nodes.datas[node.private_field_id].left;
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstList) {count, (AstId *) &ast->extra.ptr[extra]};
}

static inline AstId get_ast_unary(AstId node, Ast const *ast) {
    return (AstId) {ast->nodes.datas[node.private_field_id].left};
}

static inline AstBinary get_ast_binary(AstId node, Ast const *ast) {
    return (AstBinary) {
        {ast->nodes.datas[node.private_field_id].left},
        {ast->nodes.datas[node.private_field_id].right},
    };
}

static inline int64_t get_ast_int(AstId node, Ast const *ast) {
    return load_i64(
        ast->nodes.datas[node.private_field_id].left,
        ast->nodes.datas[node.private_field_id].right
    );
}

static inline double get_ast_float(AstId node, Ast const *ast) {
    return load_f64(
        ast->nodes.datas[node.private_field_id].left,
        ast->nodes.datas[node.private_field_id].right
    );
}

static inline AstFunction get_ast_function(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstFunction) {
        .type_param_count = ast->extra.ptr[extra],
        .type_params = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 1]],
        .param_count = ast->extra.ptr[extra + 2],
        .params = (AstId const *) &ast->extra.ptr[ast->extra.ptr[extra + 3]],
        .ret = {ast->extra.ptr[extra + 4]},
        .body = {ast->nodes.datas[node.private_field_id].left},
    };
}

static inline AstFunction get_ast_extern_function(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstFunction) {
        .param_count = ast->nodes.datas[node.private_field_id].left,
        .params = (AstId const *) &ast->extra.ptr[extra + 1],
        .ret = {ast->extra.ptr[extra]},
    };
}

static inline AstStruct get_ast_struct(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstStruct) {
        .field_count = ast->nodes.datas[node.private_field_id].left,
        .fields = (AstId const *) &ast->extra.ptr[extra],
    };
}

static inline AstEnum get_ast_enum(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstEnum) {
        .repr = {ast->nodes.datas[node.private_field_id].left},
        .member_count = ast->extra.ptr[extra],
        .members = (AstId const *) &ast->extra.ptr[extra + 1],
    };
}

static inline AstNewtype get_ast_newtype(AstId node, Ast const *ast) {
    return (AstNewtype) {
        .count = ast->nodes.datas[node.private_field_id].left,
        .type = {ast->nodes.datas[node.private_field_id].right},
    };
}

static inline AstCall get_ast_call(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstCall) {
        .operand = {ast->extra.ptr[extra]},
        .arg_count = ast->nodes.datas[node.private_field_id].left,
        .args = (AstId const *) &ast->extra.ptr[extra + 1],
    };
}

static inline AstIf get_ast_if(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstIf) {
        .condition = {ast->nodes.datas[node.private_field_id].left},
        .true_block = {ast->extra.ptr[extra]},
        .false_block = {ast->extra.ptr[extra + 1]},
    };
}

static inline AstFor get_ast_for(AstId node, Ast const *ast) {
    int32_t extra = ast->nodes.datas[node.private_field_id].right;
    return (AstFor) {
        .init = {ast->extra.ptr[extra]},
        .condition = {ast->extra.ptr[extra + 1]},
        .next = {ast->extra.ptr[extra + 2]},
        .block = {ast->nodes.datas[node.private_field_id].left},
    };
}
