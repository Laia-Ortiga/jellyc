#pragma once

#include "tir2mir.h"

typedef struct {
    Mir *mir;
} GenInput;

void gen_c(GenInput *in, Target target);
void gen_llvm(GenInput *in, Target target);
