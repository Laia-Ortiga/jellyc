#include "lex.h"

#include "adt.h"

#include <stdbool.h>
#include <stdint.h>

static String const keywords[] = {
    #define KEYWORD(keyword) {sizeof(#keyword) - 1, #keyword},
    #include "keyword-defs"
    {0},
};

#define ALPHABET_MAX 26

// Store keywords by the first letter so only a small subset of them have to be checked.
static unsigned char keywords_offsets[ALPHABET_MAX + 1];

static bool is_digit(int c) {
    return c >= '0' && c <= '9';
}

static bool is_hex_digit(int c) {
    return is_digit(c)
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

static bool is_alpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// Do not use isalnum. It depends on locale.
static bool is_id_char(int c) {
    return is_alpha(c) || is_digit(c);
}

static int peek(Lexer *lexer) {
    if (lexer->cursor.index == lexer->source.len) {
        return -1;
    }

    return (unsigned char) lexer->source.ptr[lexer->cursor.index];
}

static int consume(Lexer *lexer) {
    int c = peek(lexer);
    if (lexer->cursor.index < lexer->source.len) {
        lexer->cursor.index++;
    }
    return c;
}

static bool accept(Lexer *lexer, int c) {
    if (peek(lexer) != c) {
        return 0;
    }
    consume(lexer);
    return 1;
}

static void comment(Lexer *lexer) {
    for (;;) {
        int c = peek(lexer);
        if (c == -1 || c == '\n') {
            break;
        }
        consume(lexer);
    }
}

static TokenTag lit_string(Lexer *lexer, char terminator) {
    bool escape = 0;
    for (;;) {
        int c = peek(lexer);
        if (!escape && c == terminator) {
            consume(lexer);
            break;
        }
        if (c == -1 || c == '\n') {
            break;
        }
        escape = !escape && c == '\\';
        consume(lexer);
    }
    return TOK_STRING;
}

static TokenTag resolve_tag(String s) {
    if (s.ptr[0] >= 'a' && s.ptr[0] <= 'z') {
        int index = s.ptr[0] - 'a';
        int first = keywords_offsets[index];
        int count = keywords_offsets[index + 1] - first;
        for (int i = 0; i < count; i++) {
            if (equals(keywords[first + i], s)) {
                return first + i;
            }
        }
    }

    return TOK_ID;
}

static TokenTag builtin_id(Lexer *lexer) {
    if (is_alpha(peek(lexer))) {
        consume(lexer);
        while (is_id_char(peek(lexer))) {
            consume(lexer);
        }
    }
    return TOK_BUILTIN_ID;
}

static TokenTag id(Lexer *lexer, SourceIndex start) {
    while (is_id_char(peek(lexer))) {
        consume(lexer);
    }
    String s = {lexer->cursor.index - start.index, &lexer->source.ptr[start.index]};
    return resolve_tag(s);
}

static TokenTag number(Lexer *lexer, int c) {
    if (c == '0' && (accept(lexer, 'X') || accept(lexer, 'x'))) {
        while (is_hex_digit(peek(lexer))) {
            consume(lexer);
        }
        return TOK_HEX_INT;
    }

    while (is_digit(peek(lexer))) {
        consume(lexer);
    }

    if (accept(lexer, '.')) {
        if (!is_digit(consume(lexer))) {
            return TOK_INVALID;
        }
        while (is_digit(peek(lexer))) {
            consume(lexer);
        }
        if (accept(lexer, 'e')) {
            accept(lexer, '-');
            if (!is_digit(consume(lexer))) {
                return TOK_INVALID;
            }
            while (is_digit(peek(lexer))) {
                consume(lexer);
            }
        }
        return TOK_FLOAT;
    }

    return TOK_INT;
}

int init_lex_module(void) {
    int index = 0;
    unsigned char i = 0;
    while (i < 128 && keywords[i].ptr) {
        if (keywords[i].ptr[0] < 'a' || keywords[i].ptr[0] > 'z') {
            // Keyword must start in an alphabetical lower case letter.
            return 1;
        }

        int new_index = (unsigned char) (keywords[i].ptr[0] - 'a');
        if (new_index < index) {
            // Keywords must be sorted in alphabetical order.
            return 1;
        }

        for (int j = index; j < new_index; j++) {
            keywords_offsets[j + 1] = i;
        }

        index = new_index;
        i++;
    }

    for (int j = index + 1; j <= ALPHABET_MAX; j++) {
        keywords_offsets[j] = i;
    }

    return 0;
}

Lexer new_lexer(String source) {
    Lexer lexer = {0};
    lexer.source = source;
    return lexer;
}

Token next_token(Lexer *lexer) {
    bool found_newline = false;
    for (;;) {
        SourceIndex start = lexer->cursor;
        int c = consume(lexer);
        switch (c) {
            case -1: return (Token) {.tag = TOK_SENTINEL, .start = start, .end = start};
            case '\t': break;
            case '\n': found_newline = true; break;
            case ' ': break;
            case '#': comment(lexer); break;
            case '(': return (Token) {.comes_after_newline = found_newline, .tag = TOK_ROUNDL, .start = start, .end = lexer->cursor};
            case ')': return (Token) {.comes_after_newline = found_newline, .tag = TOK_ROUNDR, .start = start, .end = lexer->cursor};
            case '[': return (Token) {.comes_after_newline = found_newline, .tag = TOK_SQUAREL, .start = start, .end = lexer->cursor};
            case ']': return (Token) {.comes_after_newline = found_newline, .tag = TOK_SQUARER, .start = start, .end = lexer->cursor};
            case '{': return (Token) {.comes_after_newline = found_newline, .tag = TOK_CURLYL, .start = start, .end = lexer->cursor};
            case '}': return (Token) {.comes_after_newline = found_newline, .tag = TOK_CURLYR, .start = start, .end = lexer->cursor};
            case ',': return (Token) {.comes_after_newline = found_newline, .tag = TOK_COMMA, .start = start, .end = lexer->cursor};
            case '.': return (Token) {.comes_after_newline = found_newline, .tag = TOK_DOT, .start = start, .end = lexer->cursor};
            case ':': return (Token) {.comes_after_newline = found_newline, .tag = TOK_COLON, .start = start, .end = lexer->cursor};
            case ';': return (Token) {.comes_after_newline = found_newline, .tag = TOK_SEMICOLON, .start = start, .end = lexer->cursor};
            case '@': return (Token) {.comes_after_newline = found_newline, .tag = TOK_ADDRESS, .start = start, .end = lexer->cursor};
            case '+': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_ADD : TOK_ADD, .start = start, .end = lexer->cursor};
            case '-': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_SUB : accept(lexer, '>') ? TOK_ARROW : TOK_SUB, .start = start, .end = lexer->cursor};
            case '*': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_MUL : TOK_MUL, .start = start, .end = lexer->cursor};
            case '/': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_DIV : TOK_DIV, .start = start, .end = lexer->cursor};
            case '%': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_MOD : TOK_MOD, .start = start, .end = lexer->cursor};
            case '&': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_AND : TOK_AND, .start = start, .end = lexer->cursor};
            case '|': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_OR : TOK_OR, .start = start, .end = lexer->cursor};
            case '^': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_ASSIGN_XOR : TOK_XOR, .start = start, .end = lexer->cursor};
            case '=': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_EQ : TOK_ASSIGN, .start = start, .end = lexer->cursor};
            case '!': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_NE : TOK_NOT, .start = start, .end = lexer->cursor};
            case '<': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_LE : accept(lexer, '<') ? TOK_SHL : TOK_LT, .start = start, .end = lexer->cursor};
            case '>': return (Token) {.comes_after_newline = found_newline, .tag = accept(lexer, '=') ? TOK_GE : accept(lexer, '>') ? TOK_SHR : TOK_GT, .start = start, .end = lexer->cursor};
            case '\'': lit_string(lexer, '\''); return (Token) {.comes_after_newline = found_newline, .tag = TOK_CHAR, .start = start, .end = lexer->cursor};
            case '"': lit_string(lexer, '"'); return (Token) {.comes_after_newline = found_newline, .tag = TOK_STRING, .start = start, .end = lexer->cursor};
            case '`': return (Token) {.comes_after_newline = found_newline, .tag = builtin_id(lexer), .start = start, .end = lexer->cursor};
            default: {
                if (is_alpha(c)) {
                    return (Token) {.comes_after_newline = found_newline, .tag = id(lexer, start), .start = start, .end = lexer->cursor};
                }

                if (is_digit(c)) {
                    return (Token) {.comes_after_newline = found_newline, .tag = number(lexer, c), .start = start, .end = lexer->cursor};
                }

                return (Token) {.comes_after_newline = found_newline, .tag = TOK_INVALID, .start = start, .end = lexer->cursor};
            }
        }
    }
}

void poison_lexer(Lexer *lexer) {
    lexer->cursor.index = lexer->source.len;
}

char const *token_tag_to_string(TokenTag tag) {
    switch (tag) {
        #define KEYWORD(keyword) case TOK_KW_##keyword: return #keyword;
        #include "keyword-defs"

        #define TOKEN(token, string) case TOK_##token: return string;
        #include "token-defs"
    }

    return "(invalid)";
}

static int32_t find_id_length(String source, SourceIndex where) {
    int32_t end = where.index + 1;
    while (end < source.len && is_id_char((unsigned char) source.ptr[end])) {
        end++;
    }
    return end - where.index;
}

String id_token_to_string(String source, SourceIndex where) {
    return (String) {find_id_length(source, where), &source.ptr[where.index]};
}

int64_t string_token_byte_length(String source, SourceIndex where) {
    bool escape = false;
    int64_t length = 0;
    for (int32_t i = where.index + 1; i < source.len; i++) {
        char c = source.ptr[i];
        if (!escape && c == '"') {
            break;
        }
        if (escape && c == 'x') {
            length -= 2;
        }
        escape = !escape && c == '\\';
        if (!escape) {
            length++;
        }
    }
    return length;
}
