#pragma once

#include "tir2mir.h"
#include "type-analysis.h"

typedef struct {
    Declarations declarations;
    TirDependencies global_deps;
    LocalTir *insts;
    MirResult *mir_result;
} GenInput;

void gen_c(GenInput *input, Target target);
void gen_llvm(GenInput *input, Target target, Arena scratch);
