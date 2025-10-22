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

typedef IdType FileId;
typedef IdType ModuleId;
typedef IdType GlobalId;
typedef IdType LocalId;

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
    SYM_RESERVED,
    SYM_PRIVATE_GLOBAL,
    SYM_PUBLIC_GLOBAL,
    SYM_LOCAL,
} SymbolKind;

typedef enum {
    RESERVED_ERROR = 0,

    RESERVED_TYPE_START = INT_MIN,
    RESERVED_VOID = RESERVED_TYPE_START,
    #define TYPE(type) RESERVED_##type,
    #include "simple-types"
    RESERVED_TYPE_END,

    RESERVED_MACRO_START = RESERVED_TYPE_END,
    RESERVED_ALIGNOF = RESERVED_MACRO_START,
    RESERVED_SIZEOF,
    RESERVED_CAST,
    RESERVED_ZERO_EXTEND,
    RESERVED_SLICE,
    RESERVED_AFFINE,
    RESERVED_ARRAY_LENGTH_TYPE,
    RESERVED_MACRO_END,

    RESERVED_SIZE = 0,
    RESERVED_ALIGNMENT,
    RESERVED_INTERNAL_COUNT,
} ReservedTerm;

typedef struct {
    SymbolKind kind;
    union {
        ReservedTerm reserved;
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

typedef struct {
    Files files;
    Modules modules;
    HashTable *reserved;
} Scopes;
