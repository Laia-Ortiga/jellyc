#pragma once

#include "data/mir.h"
#include "fwd.h"

#include <stdint.h>

typedef struct {
    MirId left;
    MirId right;
} MirBinary;

typedef struct {
    MirId operand;
    int32_t index;
} MirAccess;

typedef struct {
    TypeId type;
    union {
        // Used for values.
        struct {
            int32_t left;
            int32_t right;
        } raw;

        MirBinary binary;
        MirAccess mir_const;
        MirId unary;
        ValueId tir_value;
    };
} MirData;

typedef struct {
    SumVec(MirData) mir;
    Vec(int32_t) extra;
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

static inline MirTag get_mir_tag(Mir *mir, MirId mir_id) {
    return mir->mir.tags[mir_id.private_field_id];
}

static inline TypeId get_mir_type(Mir *mir, MirId mir_id) {
    return mir->mir.datas[mir_id.private_field_id].type;
}

static inline MirId get_mir_unary(Mir *mir, MirId mir_id) {
    return mir->mir.datas[mir_id.private_field_id].unary;
}

static inline int64_t get_mir_int(Mir *mir, MirId mir_id) {
    uint32_t low = mir->mir.datas[mir_id.private_field_id].raw.left;
    uint32_t high = mir->mir.datas[mir_id.private_field_id].raw.right;
    return (int64_t) ((uint64_t) low | ((uint64_t) high << 32));
}

static inline MirBinary get_mir_binary(Mir *mir, MirId mir_id) {
    return mir->mir.datas[mir_id.private_field_id].binary;
}

static inline MirAccess get_mir_access(Mir *mir, MirId mir_id) {
    return mir->mir.datas[mir_id.private_field_id].mir_const;
}

static inline ValueId get_mir_tir_value(Mir *mir, MirId mir_id) {
    return mir->mir.datas[mir_id.private_field_id].tir_value;
}

static inline int32_t get_mir_extra(Mir *mir, int32_t index) {
    return mir->extra.ptr[index];
}
