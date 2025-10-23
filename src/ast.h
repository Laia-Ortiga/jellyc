#pragma once

#include "lex.h"
#include "ids.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Ast Ast;

#include "ast-types.h"

struct Ast {
    SumVecTable(AstId, AstData) nodes;
    Vec(int32_t) extra;
};

static AstId const ast_null = {0};

static inline bool ast_is_null(AstId ast_id) {
    return id_as_index(ast_id) == id_as_index(ast_null);
}

static inline AstTag ast_get_tag(Ast *ast, AstId node) {
    return nth(ast->nodes.tag_table, node);
}

static inline SourceIndex ast_get_token(Ast *ast, AstId node) {
    return (SourceIndex) {nth(ast->nodes.data_table, node).a};
}

static inline AstId ast_new(Ast *ast, AstTag tag, AstData data) {
    AstId node = {ast->nodes.len};
    sum_vec_push(&ast->nodes, data, tag);
    return node;
}
