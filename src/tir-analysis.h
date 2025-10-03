#pragma once

#include "arena.h"
#include "fwd.h"
#include "ast.h"
#include "tir.h"
#include "type-analysis.h"

typedef struct {
    Paths paths;
    Sources sources;
    Asts asts;
    AstRefs ast_refs;
    FunctionRef *functions;
    Tir *global_deps;
    LocalTir *insts;
    int32_t function_count;
} SubstructuralAnalysisInput;

int check_substructural_types(SubstructuralAnalysisInput *input, Arena scratch);
