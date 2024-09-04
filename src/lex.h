#pragma once

#include "adt.h"
#include "enums.h"

#include <stdint.h>

typedef struct {
    int32_t index;
} SourceIndex;

typedef struct {
    int comes_after_newline;
    TokenTag tag;
    SourceIndex start;
    SourceIndex end;
} Token;

typedef struct {
    String source;
    SourceIndex cursor;
} Lexer;

int init_lex_module(void);
Lexer new_lexer(String source);
Token next_token(Lexer *lexer);
void poison_lexer(Lexer *lexer);
char const *token_tag_to_string(TokenTag tag);
String id_token_to_string(String source, SourceIndex where);
int64_t string_token_byte_length(String source, SourceIndex where);
