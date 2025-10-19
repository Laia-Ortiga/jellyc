#pragma once

#include "arena.h"
#include "ast.h"
#include "mir.h"
#include "tir.h"
#include "fwd.h"

typedef struct {
    Target target;
    Paths paths;
    Sources sources;
    Asts asts;
    AstRefs ast_refs;
    TirId *functions;
    Tir *global_deps;
    LocalTir *insts;
    int32_t function_count;
} MirAnalysisInput;

Mir tir_to_mir(MirAnalysisInput *input, Arena *permanent, Arena scratch);
