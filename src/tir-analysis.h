#pragma once

#include "arena.h"
#include "fwd.h"
#include "data/ast.h"
#include "data/tir.h"

typedef struct {
    char **paths;
    String *sources;
    Ast *asts;
    AstRef *ast_refs;
    DefId *functions;
    TirDependencies *global_deps;
    LocalTir *insts;
    int32_t function_count;
} SubstructuralAnalysisInput;

int check_substructural_types(SubstructuralAnalysisInput *input, Arena scratch);
