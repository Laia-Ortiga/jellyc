#pragma once

#include "data/tir.h"
#include "fwd.h"
#include "lex.h"

typedef struct {
    char const *path;
    String source;
    SourceIndex where;
    int32_t len;
    SourceIndex mark;
} SourceLoc;

typedef struct {
    ErrorKind kind;
    union {
        TokenTag expected_token;
        struct {
            TirContext ctx;
            TypeId type;
            int32_t extra;
        } type_error;
        struct {
            TirContext ctx;
            TypeId type1;
            TypeId type2;
        } double_type_error;
        struct {
            int32_t expected;
            int32_t provided;
        } count_error;
    };
} Diagnostic;

void init_diagnostic_module(void);
void print_diagnostic(SourceLoc const *loc, Diagnostic const *diagnostic);
