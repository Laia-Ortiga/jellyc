#pragma once

#include "fwd.h"

static inline char **nth_path(Paths paths, FileId file) {
    return &paths.private_field_id[file.private_field_id];
}

static inline String *nth_source(Sources sources, FileId file) {
    return &sources.private_field_id[file.private_field_id];
}

static inline Ast *nth_ast(Ast *asts, FileId file) {
    return &asts[file.private_field_id];
}

static inline File *nth_file(File *files, FileId file) {
    return &files[file.private_field_id];
}

static inline Module *nth_module(Module *modules, ModuleId m) {
    return &modules[m.private_field_id];
}

#define nth(list, index)            \
    (*_Generic((list),              \
        Paths: nth_path,            \
        Sources: nth_source,        \
        Ast*: nth_ast,              \
        File*: nth_file,            \
        Module*: nth_module,        \
        default: (0)                \
    )(list, index))
