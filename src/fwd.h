#pragma once

#include "data/ast.h"
#include "hash.h"

#include <limits.h>
#include <stdint.h>

typedef enum {
    BACKEND_C,
    BACKEND_LLVM,
} Backend;

typedef enum {
    TARGET_ISIZE_64,
    TARGET_ISIZE_32,
} Target;

typedef struct {
    Backend backend;
    Target target;
    bool print_debug;
} Options;

typedef struct {
    int32_t module;
    HashTable scope;
} File;

typedef struct {
    HashTable public_scope;
    HashTable private_scope;
} Module;

typedef struct {
    int32_t id;
} DefId;

typedef struct {
    int32_t id;
} LocalId;

typedef enum {
    SYM_UNDEFINED,
    SYM_BUILTIN,
    SYM_GLOBAL,
    SYM_LOCAL,
} SymbolKind;

typedef enum {
    TYPE_INVALID,

    BUILTIN_TYPE_START = INT_MIN,
    TYPE_VOID = BUILTIN_TYPE_START,
    #define TYPE(type) TYPE_##type,
    #include "simple-types"
    BUILTIN_TYPE_END,

    BUILTIN_MACRO_START = BUILTIN_TYPE_END,
    BUILTIN_ALIGNOF = BUILTIN_MACRO_START,
    BUILTIN_SIZEOF,
    BUILTIN_CAST,
    BUILTIN_ZERO_EXTEND,
    BUILTIN_SLICE,
    BUILTIN_AFFINE,
    BUILTIN_ARRAY_LENGTH_TYPE,
    BUILTIN_MACRO_END,

    BUILTIN_TERM_END = BUILTIN_MACRO_END,

    TERM_COUNT = 1,
    BUILTIN_SIZE = TERM_COUNT,
    BUILTIN_ALIGNMENT,
    TERM_GLOBAL_COUNT,
} PrimitiveTerm;

typedef struct {
    SymbolKind kind;
    union {
        PrimitiveTerm builtin;
        DefId global;
        LocalId local;
    };
} Symbol;

typedef struct {
    AstId node;
    int32_t file;
} AstRef;

typedef Vec(AstRef) AstRefVec;
typedef Vec(DefId) DefVec;
