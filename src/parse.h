#pragma once

#include "adt.h"
#include "data/ast.h"
#include "diagnostic.h"
#include "lex.h"

typedef struct {
    ErrorKind kind;
    TokenTag token;
    SourceIndex start;
    SourceIndex end;
} ParseError;

typedef Vec(ParseError) ParseErrorList;

typedef struct {
    char const *path;
    String source;

    Ast *ast;
    ParseErrorList *errors;
} ParseInfo;

int parse_ast(ParseInfo *result);
