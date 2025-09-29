#pragma once

#include "adt.h"

#include <stdint.h>

typedef enum {
    // Stack Frame

    MIR_ALLOC,
    MIR_ALLOC_VAR,
    MIR_STACK_COPY,
    MIR_STACK_COPY_AT,
    MIR_STACK_POP,

    // Value

    MIR_INT,
    MIR_TIR_VALUE,

    // ( a b -- )
    MIR_ASSIGN,

    /*
        Unary
        ( a -- r )
    */

    MIR_NEG,
    MIR_NOT,
    MIR_ADDRESS,
    MIR_DEREF,

    /*
        Binary
        ( a b -- r )
    */

    MIR_ADD,
    MIR_SUB,
    MIR_MUL,
    MIR_DIV,
    MIR_MOD,
    MIR_AND,
    MIR_OR,
    MIR_XOR,
    MIR_SHL,
    MIR_SHR,
    MIR_EQ,
    MIR_NE,
    MIR_LT,
    MIR_GT,
    MIR_LE,
    MIR_GE,
    MIR_INDEX,
    MIR_SLICE_INDEX,

    /*
        Cast
        ( a -- r )
        data ( type -- )
    */

    MIR_ITOF,
    MIR_ITRUNC,
    MIR_SEXT,
    MIR_ZEXT,
    MIR_FTOI,
    MIR_FTRUNC,
    MIR_FEXT,
    MIR_PTR_CAST,

    /*
        Access
        ( a -- r )
        data ( field -- )
    */
    MIR_ACCESS,

    MIR_CALL,

    // Control Flow

    MIR_BR,
    MIR_BR_IF,
    MIR_BR_IF_NOT,
    MIR_RET_VOID,
    MIR_RET,
} MirTag;

typedef struct {
    Vec(unsigned char) insts;
    Vec(int32_t) data;
} Mir;

static inline bool is_mir_terminator(MirTag tag) {
    switch (tag) {
        case MIR_BR:
        case MIR_BR_IF:
        case MIR_BR_IF_NOT:
        case MIR_RET_VOID:
        case MIR_RET: return true;

        default: return false;
    }
}
