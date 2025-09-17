#pragma once

#include "adt.h"
#include "data/tir.h"

#include <stdint.h>

typedef enum {
    // Statement

    MIR_PARAM,
    MIR_ALLOC,
    MIR_ASSIGN,

    // Value

    MIR_INT,
    MIR_STRING,
    MIR_TIR_VALUE,

    // Pointer operator

    MIR_ADDRESS,
    MIR_DEREF,

    // Arithmetic operator

    MIR_MINUS,
    MIR_ADD,
    MIR_SUB,
    MIR_MUL,
    MIR_DIV,
    MIR_MOD,

    // Bitwise operator

    MIR_NOT,
    MIR_AND,
    MIR_OR,
    MIR_XOR,
    MIR_SHL,
    MIR_SHR,

    // Comparator

    MIR_EQ,
    MIR_NE,
    MIR_LT,
    MIR_GT,
    MIR_LE,
    MIR_GE,

    // Cast

    MIR_ITOF,
    MIR_ITRUNC,
    MIR_SEXT,
    MIR_ZEXT,
    MIR_FTOI,
    MIR_FTRUNC,
    MIR_FEXT,
    MIR_PTR_CAST,

    // Access

    MIR_INDEX,
    MIR_SLICE_INDEX,
    MIR_ACCESS,

    // Control flow

    MIR_START_CALL,
    MIR_END_CALL,
    MIR_ARG,
    MIR_BR,
    MIR_BR_IF,
    MIR_BR_IF_NOT,
    MIR_RET_VOID,
    MIR_RET,
} MirTag;

typedef struct {
    int32_t private_field_id;
} MirId;

typedef struct {
    MirId left;
    MirId right;
} MirBinary;

typedef struct {
    MirId operand;
    int32_t index;
} MirAccess;

typedef struct {
    TirId type;
    union {
        // Used for values.
        struct {
            int32_t left;
            int32_t right;
        } raw;

        MirBinary binary;
        MirAccess mir_const;
        MirId unary;
        TirId tir_value;
    };
} MirData;

typedef SumVec(MirData) Mir;

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

static inline MirTag get_mir_tag(Mir *mir, MirId mir_id) {
    return mir->tags[mir_id.private_field_id];
}

static inline TirId get_mir_type(Mir *mir, MirId mir_id) {
    return mir->datas[mir_id.private_field_id].type;
}

static inline MirId get_mir_unary(Mir *mir, MirId mir_id) {
    return mir->datas[mir_id.private_field_id].unary;
}

static inline int64_t get_mir_int(Mir *mir, MirId mir_id) {
    uint32_t low = mir->datas[mir_id.private_field_id].raw.left;
    uint32_t high = mir->datas[mir_id.private_field_id].raw.right;
    return (int64_t) ((uint64_t) low | ((uint64_t) high << 32));
}

static inline MirBinary get_mir_binary(Mir *mir, MirId mir_id) {
    return mir->datas[mir_id.private_field_id].binary;
}

static inline MirAccess get_mir_access(Mir *mir, MirId mir_id) {
    return mir->datas[mir_id.private_field_id].mir_const;
}

static inline TirId get_mir_tir_value(Mir *mir, MirId mir_id) {
    return mir->datas[mir_id.private_field_id].tir_value;
}
