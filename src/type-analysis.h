#pragma once

#include "arena.h"
#include "fwd.h"
#include "tir.h"

typedef struct {
    Options *options;
    int file_count;
    Paths paths;
    Sources sources;
    Asts asts;
    Files files;
    HashTable *module_table;
    Modules modules;
    HashTable *global_scope;
    AstRefs ast_refs;
    int32_t def_count;

    int32_t function_body_count;
} TirInput;

typedef struct {
    AstRef ast_ref;
    TirId value;
} FunctionRef;

typedef struct {
    int32_t body_first;
    int32_t body_length;
    Tir deps;
    int32_t local_count;
} LocalTir;

typedef struct {
    Tir global_deps;
    LocalTir *insts;
    FunctionRef *functions;
    int error;
} TirOutput;

Symbol lookup_module_global(Scopes *scopes, ModuleId module, String name);
Symbol lookup_global(Scopes *scopes, FileId file, String name);
TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch);
