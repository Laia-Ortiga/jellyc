#pragma once

#include "mir.h"

typedef enum {
    MIR_OPERAND_INT,
    MIR_OPERAND_FLOAT,
    MIR_OPERAND_NULL,
    MIR_OPERAND_STRING,
    MIR_OPERAND_VARIABLE,
    MIR_OPERAND_GLOBAL,
    MIR_OPERAND_TMP,
} MirOperandTag;

typedef struct {
    bool is_lvalue;
    MirOperandTag tag : 8;
    MirTypeId type;
    union {
        int64_t i;
        double f;
        char const *s;
        int32_t index;
    };
} MirOperand;
