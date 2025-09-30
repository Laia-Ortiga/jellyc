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
    Tir global_deps;
    LocalTir *insts;
    GlobalId *functions;
    int error;
} TirOutput;

TirOutput analyze_types(TirInput *input, Arena *permanent, Arena scratch);
