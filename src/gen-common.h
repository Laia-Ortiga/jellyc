#pragma once

#include "tir.h"

typedef enum {
    MIR_OPERAND_INT,
    MIR_OPERAND_TIR,
    MIR_OPERAND_TMP,
} MirOperandTag;

typedef struct {
    bool is_lvalue;
    MirOperandTag tag : 8;
    TirId type;
    union {
        int64_t i;
        TirId value;
        int32_t index;
    };
} MirOperand;
