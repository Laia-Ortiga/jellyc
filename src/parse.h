#pragma once

#include "adt.h"
#include "data/ast.h"

int parse_ast(Ast *result, char const *path, String source);
