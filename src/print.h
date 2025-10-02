#pragma once

#include "ast.h"
#include "tir.h"

#include <stdio.h>

void print_ast(char const *path, String source, Ast *ast);
void print_node(String source, Ast *ast, AstId node);
void print_tir(
    TirContext c,
    char const *name,
    int32_t first,
    int32_t length
);
void print_tir_term(TirContext c, TirId term);

void print_type(FILE *file, TirContext c, TirId type);
void debug_type(TirContext c, TirId type);
