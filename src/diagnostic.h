#pragma once

#include "tir.h"
#include "lex.h"

typedef enum {
    #define VARIANT(name, ...) DIAGNOSTIC_##name,
    #define X(...)
    #define EXTRA(...) __VA_ARGS__,
    #include "diagnostic-defs"
} ErrorKind;

typedef struct {
    char const *path;
    String source;
    SourceIndex where;
    int32_t len;
    SourceIndex mark;
} SourceLoc;

#define VARIANT(name, ...) typedef struct { __VA_ARGS__ } Diagnostic##name;
#define X(name, type) type name;
#define EXTRA(...)
#include "diagnostic-defs"

typedef struct {
    ErrorKind kind;
    union {
#define VARIANT(name, ...) Diagnostic##name Diagnostic##name;
#define X(name, type) type name;
#define EXTRA(...)
#include "diagnostic-defs"
    };
} Diagnostic;

void print_diagnostic(SourceLoc const *loc, Diagnostic const *diagnostic);

#define Diagnostic(name, ...)               \
    ((Diagnostic) {                         \
        .kind = DIAGNOSTIC_##name,          \
        .Diagnostic##name = __VA_ARGS__,    \
    })
