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

static AstId const null_ast = {0};

static inline bool is_ast_null(AstId ast_id) {
    return !ast_id.private_field_id;
}

static inline AstTag get_ast_tag(Ast *ast, AstId node) {
    return nth(ast->nodes.tag_table, node);
}

static inline SourceIndex get_ast_token(Ast *ast, AstId node) {
    return (SourceIndex) {nth(ast->nodes.data_table, node).a};
}

static inline AstId new_ast(Ast *ast, AstTag tag, AstData data) {
    AstId node = {ast->nodes.len};
    sum_vec_push(&ast->nodes, data, tag);
    return node;
}
