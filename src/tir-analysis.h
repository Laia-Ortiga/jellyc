#pragma once

#include "arena.h"
#include "fwd.h"
#include "ast.h"
#include "tir.h"

typedef struct {
    Paths paths;
    Sources sources;
    Ast *asts;
    AstRef *ast_refs;
    DefId *functions;
    Tir *global_deps;
    LocalTir *insts;
    int32_t function_count;
} SubstructuralAnalysisInput;

int check_substructural_types(SubstructuralAnalysisInput *input, Arena scratch);
