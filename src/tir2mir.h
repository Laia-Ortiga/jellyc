#pragma once

#include "arena.h"
#include "ast.h"
#include "mir.h"
#include "tir.h"
#include "fwd.h"
#include "type-analysis.h"

typedef struct {
    Target target;
    Paths paths;
    Sources sources;
    Asts asts;
    AstRefs ast_refs;
    TirId *functions;
    Tir *global_tir;
    LocalTir *function_tirs;
    int32_t function_count;
} MirAnalysisInput;

Mir tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch);
