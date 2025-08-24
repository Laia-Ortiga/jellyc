#pragma once

#include "arena.h"
#include "fwd.h"
#include "role-analysis.h"
#include "data/tir.h"

typedef struct {
    Vec(TermId) structs;
    Vec(TermId) extern_vars;
    Vec(TermId) extern_functions;
    Vec(TermId) functions;
    TermId main;
} Declarations;

typedef union {
    TermId value;
    TermId type;
} TirRef;

typedef struct {
    Options *options;
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    HashTable *module_table;
    Module *modules;
    HashTable *global_scope;
    AstRef *ast_refs;
    int32_t def_count;
    int32_t order_count;

    Locals *local_ast_refs;
    DefId *order;
    Rir *rirs;
    DefId *functions;
    int32_t function_count;
} TirInput;

typedef struct {
    Declarations declarations;
    TirDependencies global_deps;
    LocalTir *insts;
    int error;
} TirOutput;

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch);
