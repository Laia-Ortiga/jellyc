#pragma once

#include "arena.h"
#include "ast.h"
#include "mir.h"
#include "tir.h"
#include "fwd.h"

typedef struct {
    Paths paths;
    Sources sources;
    Ast *asts;
    AstRef *ast_refs;
    TirId *functions;
    Tir *global_deps;
    LocalTir *insts;
    int32_t function_count;
} MirAnalysisInput;

typedef struct {
    Mir mir;
    int32_t *ends;
    int32_t *data_starts;
} MirResult;

MirResult tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch);
