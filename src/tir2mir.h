#pragma once

#include "arena.h"
#include "data/ast.h"
#include "data/mir.h"
#include "data/tir.h"

typedef struct {
    char **paths;
    String *sources;
    Ast *asts;
    AstRef *ast_refs;
    ValueId *functions;
    TirDependencies *global_deps;
    LocalTir *insts;
    int32_t function_count;
} MirAnalysisInput;

typedef struct {
    Mir mir;
    int32_t *ends;
} MirResult;

MirResult tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch);
