#pragma once

#include "arena.h"
#include "ast.h"
#include "fwd.h"
#include "tir.h"

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

    int32_t function_body_count;
} TirInput;

typedef struct {
    Tir global_deps;
    LocalTir *insts;
    DefId *functions;
    int error;
} TirOutput;

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch);
