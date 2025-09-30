#pragma once

#include "adt.h"
#include "ast.h"
#include "diagnostic.h"
#include "lex.h"

typedef struct {
    SourceIndex start;
    SourceIndex end;
    Diagnostic diag;
} ParseError;

typedef Vec(ParseError) ParseErrorList;

typedef struct {
    char const *path;
    String source;

    Ast *ast;
    ParseErrorList *errors;
} ParseInfo;

int parse_ast(ParseInfo *result);
