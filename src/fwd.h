#pragma once

#include "data/ast.h"
#include "enums.h"
#include "hash.h"

#include <stdint.h>

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
