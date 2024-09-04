#pragma once

#include "arena.h"
#include "fwd.h"
#include "data/ast.h"
#include "data/rir.h"

typedef Vec(LocalAstRef) Locals;

typedef struct {
    int file_count;
    char **paths;
    String *sources;
    Ast *asts;
    File *files;
    HashTable *module_table;
    Module *modules;
    HashTable *global_scope;
    Rir *rirs;
    AstRef *ast_refs;
    DefId *functions;
    int32_t def_count;
    int32_t function_count;
} RirTopInput;

typedef struct {
    unsigned char *rir_refs;
    DefId *order;
    Locals *local_ast_refs;
    int32_t count;
    int error;
} RirTopOutput;

RirTopOutput analyze_roles(RirTopInput *input, Arena *permanent, Arena scratch);
