#pragma once

#include "adt.h"
#include "ast.h"
#include "ids.h"
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
    int32_t private_field_id;
} FileId;

typedef struct {
    int32_t private_field_id;
} ModuleId;

typedef struct {
    int32_t private_field_id;
} GlobalId;

typedef struct {
    int32_t private_field_id;
} LocalId;

static FileId const internal_file_id = {0};

typedef struct {
    ModuleId module;
    HashTable scope;
} File;

typedef struct {
    HashTable scope;
} Module;

typedef enum {
    SYM_UNDEFINED,
    SYM_BUILTIN,
    SYM_GLOBAL,
    SYM_LOCAL,
} SymbolKind;

typedef enum {
    RESERVED_ERROR = 0,

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
} ReservedTerm;

typedef struct {
    SymbolKind kind;
    union {
        ReservedTerm builtin;
        GlobalId global;
        LocalId local;
    };
} Symbol;

typedef struct {
    AstId node;
    FileId file;
} AstRef;

typedef struct {
    bool is_public;
    AstRef ref;
} AstGlobal;

typedef Table(FileId, char*) Paths;
typedef Table(FileId, String) Sources;
typedef Table(FileId, Ast) Asts;
typedef Table(FileId, File) Files;
typedef Table(ModuleId, Module) Modules;

typedef VecTable(GlobalId, AstGlobal) AstRefVec;
typedef typeof((AstRefVec) {0}.table) AstRefs;
typedef Vec(GlobalId) DefVec;
