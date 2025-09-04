#pragma once

#include "arena.h"
#include "data/ast.h"
#include "fwd.h"
#include "data/tir.h"

typedef struct {
    Options *options;
    int file_count;
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    HashTable *module_table;
    Module *modules;
    HashTable *global_scope;
    AstRef *ast_refs;
    int32_t def_count;

    DefId *functions;
    int32_t function_count;
} TirInput;

typedef struct {
    TirDependencies global_deps;
    LocalTir *insts;
    int error;
} TirOutput;

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch);
