#pragma once

#include "ast.h"
#include "tir.h"

void print_ast(char const *path, String source, Ast const *ast);
void print_tir(
    TirContext context,
    char const *name,
    int32_t first,
    int32_t length
);
