#pragma once

#include "adt.h"
#include "fwd.h"

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    #define DATA(name, type)
    #define X(name, ...) MIR_##name,
    #include "mir-defs"
} MirTag;

typedef enum {
    MIR_TYPE_I8 = -2147483648,
    MIR_TYPE_I16,
    MIR_TYPE_I32,
    MIR_TYPE_I64,
    MIR_TYPE_F32,
    MIR_TYPE_F64,
    MIR_TYPE_VOID,
    MIR_TYPE_BOOL,
    MIR_TYPE_PTR,
    MIR_TYPE_SLICE,
    MIR_TYPE_START,
} MirType;

typedef struct {
    int32_t private_field_id;
} MirTypeId;

typedef struct {
    MirTypeId elem;
    int64_t length;
} MirArrayType;

typedef struct {
    int32_t param_count;
    int32_t first_param;
    MirTypeId ret;
} MirFunctionType;

typedef struct {
    int32_t field_count;
    int32_t first_field;
    int32_t alignment;
    int64_t size;
} MirStructType;

typedef struct {
    char const *name;
    MirTypeId type;
} MirGlobal;

typedef struct {
    enum {
        MIR_TYPE_ARRAY,
        MIR_TYPE_FUNCTION,
        MIR_TYPE_STRUCT,
    } tag;
    union {
        MirArrayType array;
        MirFunctionType function;
        MirStructType struct_;
    };
} MirTypeUnion;

typedef struct {
    Vec(unsigned char) insts;
    Vec(int32_t) data;

    Vec(MirTypeId) type_extra;
    Vec(MirTypeUnion) types;
    Vec(MirGlobal) functions;
    Vec(MirGlobal) extern_functions;
    Vec(MirGlobal) extern_vars;
    int32_t main_function;
    int32_t *ends;
    int32_t *data_starts;
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

static inline bool is_mir_int_type(MirTypeId type) {
    return type.private_field_id >= MIR_TYPE_I8 && type.private_field_id <= MIR_TYPE_I64;
}

static inline bool is_mir_float_type(MirTypeId type) {
    return type.private_field_id >= MIR_TYPE_F32 && type.private_field_id <= MIR_TYPE_F64;
}

static inline MirTypeUnion get_mir_type(Mir *mir, MirTypeId type) {
    if (type.private_field_id < 0) {
        abort();
    }
    return mir->types.ptr[type.private_field_id];
}

static inline bool get_mir_type_alignment(Mir *mir, MirTypeId type, Target target) {
    switch ((MirType) type.private_field_id) {
        case MIR_TYPE_I8:
        case MIR_TYPE_BOOL: {
            return 1;
        }
        case MIR_TYPE_I16: {
            return 2;
        }
        case MIR_TYPE_I32:
        case MIR_TYPE_F32: {
            return 4;
        }
        case MIR_TYPE_I64:
        case MIR_TYPE_F64: {
            return 8;
        }
        case MIR_TYPE_VOID: {
            abort();
        }
        case MIR_TYPE_PTR:
        case MIR_TYPE_SLICE: {
            switch (target) {
                case TARGET_ISIZE_64: return 8;
                case TARGET_ISIZE_32: return 4;
            }
            abort();
        }
        default: {
            MirTypeUnion u = get_mir_type(mir, type);
            switch (u.tag) {
                case MIR_TYPE_ARRAY: {
                    return get_mir_type_alignment(mir, u.array.elem, target);
                }
                case MIR_TYPE_FUNCTION: {
                    switch (target) {
                        case TARGET_ISIZE_64: return 8;
                        case TARGET_ISIZE_32: return 4;
                    }
                    abort();
                }
                case MIR_TYPE_STRUCT: {
                    return u.struct_.alignment;
                }
            }
            abort();
        }
    }
}

static inline bool is_mir_type_aggregate(Mir *mir, MirTypeId type) {
    if (type.private_field_id < 0) {
        return type.private_field_id == MIR_TYPE_SLICE;
    }
    return mir->types.ptr[type.private_field_id].tag != MIR_TYPE_FUNCTION;
}
