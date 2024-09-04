#pragma once

#include "data/ast.h"
#include "data/tir.h"

void print_ast(char const *path, String source, Ast const *ast);
void print_tir(TirContext context, char const *name, TirInstList *tir, TirId first);
