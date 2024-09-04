#pragma once

#include "enums.h"
#include "hash.h"
#include "wrappers.h"

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
    SymbolKind kind;
    union {
        BuiltinId builtin;
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

typedef struct {
    Role role;
    AstId node;
} LocalAstRef;
