#include "parse.h"

#include "adt.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "float.h"
#include "lex.h"
#include "util.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define LINKED_LIST_SIZE 14

typedef struct ArenaLinkedNode {
    struct ArenaLinkedNode *next;
    AstId elements[LINKED_LIST_SIZE];
} ArenaLinkedNode;

typedef struct {
    int32_t count;
    ArenaLinkedNode *head;
} ArenaLinkedList;

static int32_t push_extra(Ast *ast, ArenaLinkedList list) {
    int32_t extra = ast->extra.len;
    int32_t *result = vec_grow(&ast->extra, list.count);
    int32_t node_length = (list.count - 1) % LINKED_LIST_SIZE + 1;
    int32_t index = list.count;
    for (ArenaLinkedNode *node = list.head; node; node = node->next) {
        index -= node_length;
        for (int32_t i = 0; i < node_length; i++) {
            result[index + i] = node->elements[i].private_field_id;
        }
        node_length = LINKED_LIST_SIZE;
    }
    return extra;
}

static int32_t push_extra_array(Ast *ast, int32_t count, int32_t *elements) {
    int32_t extra = ast->extra.len;
    int32_t *result = vec_grow(&ast->extra, count);
    for (int32_t i = 0; i < count; i++) {
        result[i] = elements[i];
    }
    return extra;
}

static AstId add_node(AstTag tag, AstData data, Ast *ast) {
    AstId node = {ast->nodes.len};
    sum_vec_push(&ast->nodes, data, tag);
    return node;
}

static AstId add_binary_ast(AstTag tag, SourceIndex token, AstId left, AstId right, Ast *ast) {
    return add_node(tag, (AstData) {token, left.private_field_id, right.private_field_id}, ast);
}

static AstId add_unary_ast(AstTag tag, SourceIndex token, AstId operand, Ast *ast) {
    return add_node(tag, (AstData) {token, operand.private_field_id, 0}, ast);
}

static AstId add_leaf_ast(AstTag tag, SourceIndex token, Ast *ast) {
    return add_node(tag, (AstData) {token, 0, 0}, ast);
}

static AstId add_ast_int(AstTag tag, SourceIndex token, int64_t i, Ast *ast) {
    uint32_t low;
    uint32_t high;
    store_i64(i, &low, &high);
    return add_node(tag, (AstData) {token, low, high}, ast);
}

static AstId add_ast_float(AstTag tag, SourceIndex token, double f, Ast *ast) {
    uint32_t low;
    uint32_t high;
    store_f64(f, &low, &high);
    return add_node(tag, (AstData) {token, low, high}, ast);
}

typedef enum {
    PREC_NONE,
    PREC_ASSIGN,
    PREC_LOGIC,
    PREC_CMP,
    PREC_BIT,
    PREC_ADD,
    PREC_MUL,
    PREC_AS,
    PREC_POSTFIX,
} Precedence;

typedef struct {
    char const *path;
    Lexer lexer;
    Token lookahead;
    Ast ast;
    ParseErrorList errors;
    Arena scratch;
    bool internal;
    bool error;
} Parser;

static AstId parse_block(Parser *parser);
static AstId parse_expr(Parser *parser, Precedence prec);

static void push(Parser *parser, ArenaLinkedList *list, AstId node) {
    if (list->count % LINKED_LIST_SIZE == 0) {
        ArenaLinkedNode *new_node = arena_alloc(&parser->scratch, ArenaLinkedNode, 1);
        new_node->next = list->head;
        list->head = new_node;
    }

    list->head->elements[list->count % LINKED_LIST_SIZE] = node;
    list->count++;
}

static Precedence get_precedence(TokenTag tag) {
    switch (tag) {
        #define PREC(TOKEN, NODE, P) case TOKEN: return P;
        #include "precedence-defs"

        default: return PREC_NONE;
    };
}

static AstTag bin_ast_tag(TokenTag token_tag) {
    switch (token_tag) {
        #define PREC(TOKEN, NODE, P) case TOKEN: return NODE;
        #include "precedence-defs"

        default: abort();
    }
}

static void error(Parser *parser, ParseError const *diagnostic) {
    if (parser->error) {
        return;
    }

    vec_push(&parser->errors, *diagnostic);
    poison_lexer(&parser->lexer);
    parser->lookahead = next_token(&parser->lexer);
    parser->error = true;
}

static Token next_valid_token(Parser *parser) {
    Token token = next_token(&parser->lexer);

    while (token.tag == TOK_INVALID) {
        error(parser, &(ParseError) {
            .start = token.start,
            .end = token.end,
            .diag = Diagnostic(ErrorInvalidToken, {0}),
        });
        token = next_token(&parser->lexer);
    }

    return token;
}

static Token consume(Parser *parser) {
    Token token = parser->lookahead;
    parser->lookahead = next_valid_token(parser);
    return token;
}

static bool accept(Parser *parser, TokenTag tag) {
    if (parser->lookahead.tag != tag) {
        return false;
    }

    consume(parser);
    return true;
}

static SourceIndex expect(Parser *parser, TokenTag tag) {
    if (parser->lookahead.tag != tag) {
        error(parser, &(ParseError) {
            .start = parser->lookahead.start,
            .end = parser->lookahead.end,
            .diag = Diagnostic(ErrorExpectedToken, {
                .expected = tag,
            }),
        });
    }

    return consume(parser).start;
}

static SourceIndex expect_id(Parser *parser) {
    if (parser->lookahead.tag != TOK_ID && (!parser->internal || parser->lookahead.tag != TOK_BUILTIN_ID)) {
        error(parser, &(ParseError) {
            .start = parser->lookahead.start,
            .end = parser->lookahead.end,
            .diag = Diagnostic(ErrorExpectedToken, {
                .expected = TOK_ID,
            }),
        });
    }

    return consume(parser).start;
}

static ArenaLinkedList parse_type_parameters(Parser *parser) {
    ArenaLinkedList params = {0};
    if (!accept(parser, TOK_SQUAREL)) {
        return params;
    }
    while (parser->lookahead.tag != TOK_SQUARER) {
        SourceIndex token = expect_id(parser);
        AstId node = add_leaf_ast(AST_PARAM, token, &parser->ast);
        push(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_SQUARER);
    return params;
}

static ArenaLinkedList parse_parameters(Parser *parser) {
    expect(parser, TOK_ROUNDL);
    ArenaLinkedList params = {0};
    while (parser->lookahead.tag != TOK_ROUNDR) {
        SourceIndex token = expect_id(parser);
        AstId type_node = parse_expr(parser, PREC_NONE);
        AstId node = add_unary_ast(AST_PARAM, token, type_node, &parser->ast);
        push(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_ROUNDR);
    return params;
}

static ArenaLinkedList parse_fields(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ArenaLinkedList params = {0};
    while (parser->lookahead.tag != TOK_CURLYR) {
        SourceIndex token = expect(parser, TOK_ID);
        AstId type_node = parse_expr(parser, PREC_NONE);
        AstId node = add_unary_ast(AST_PARAM, token, type_node, &parser->ast);
        push(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_CURLYR);
    return params;
}

static ArenaLinkedList parse_enum_members(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ArenaLinkedList params = {0};
    while (parser->lookahead.tag != TOK_CURLYR) {
        SourceIndex token = expect(parser, TOK_ID);
        AstId node = add_leaf_ast(AST_ID, token, &parser->ast);
        push(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_CURLYR);
    return params;
}

static AstId parse_var(Parser *parser, AstTag tag) {
    consume(parser);
    SourceIndex token = expect_id(parser);
    expect(parser, TOK_ASSIGN);
    AstId init = parse_expr(parser, PREC_NONE);
    return add_unary_ast(tag, token, init, &parser->ast);
}

static AstId parse_if(Parser *parser) {
    SourceIndex token = consume(parser).start;
    AstId cond = parse_expr(parser, PREC_NONE);
    AstId true_block = parse_block(parser);

    AstId false_block = null_ast;
    if (accept(parser, TOK_KW_else)) {
        false_block = parse_block(parser);
    }

    int32_t extra[] = {
        true_block.private_field_id,
        false_block.private_field_id,
    };
    int32_t index = push_extra_array(&parser->ast, ArrayLength(extra), extra);
    return add_node(AST_IF, (AstData) {token, cond.private_field_id, index}, &parser->ast);
}

static AstId parse_while(Parser *parser) {
    SourceIndex token = consume(parser).start;
    AstId cond = parse_expr(parser, PREC_NONE);
    AstId block = parse_block(parser);
    return add_binary_ast(AST_WHILE, token, cond, block, &parser->ast);
}

static AstId parse_for(Parser *parser) {
    consume(parser);
    SourceIndex token = expect_id(parser);
    expect(parser, TOK_ASSIGN);
    AstId init = parse_expr(parser, PREC_NONE);
    expect(parser, TOK_SEMICOLON);
    AstId cond = parse_expr(parser, PREC_NONE);
    expect(parser, TOK_SEMICOLON);
    AstId next = parse_expr(parser, PREC_NONE);
    AstId block = parse_block(parser);
    int32_t extra[] = {
        init.private_field_id,
        cond.private_field_id,
        next.private_field_id,
    };
    int32_t index = push_extra_array(&parser->ast, ArrayLength(extra), extra);
    return add_node(AST_FOR, (AstData) {token, block.private_field_id, index}, &parser->ast);
}

static ArenaLinkedList parse_switch_cases(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ArenaLinkedList cases = {0};
    while (!accept(parser, TOK_SENTINEL) && !accept(parser, TOK_CURLYR)) {
        SourceIndex token = parser->lookahead.start;
        if (parser->lookahead.tag == TOK_KW_else) {
            // default case
            consume(parser);
            expect(parser, TOK_ARROW);
            AstId value = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_COMMA);
            AstId node = add_binary_ast(AST_SWITCH_CASE, token, null_ast, value, &parser->ast);
            push(parser, &cases, node);
        } else {
            AstId cond = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_ARROW);
            AstId value = parse_expr(parser, PREC_NONE);
            expect(parser,TOK_COMMA);
            AstId node = add_binary_ast(AST_SWITCH_CASE, token, cond, value, &parser->ast);
            push(parser, &cases, node);
        }
    }
    return cases;
}

static AstId parse_switch(Parser *parser, SourceIndex token) {
    AstId cond = null_ast;
    if (parser->lookahead.tag != TOK_CURLYL) {
        cond = parse_expr(parser, PREC_NONE);
    }
    ArenaLinkedList items = parse_switch_cases(parser);
    int32_t extra = push_extra_array(&parser->ast, 1, &cond.private_field_id);
    push_extra(&parser->ast, items);
    return add_node(AST_SWITCH, (AstData) {token, items.count, extra}, &parser->ast);
}

static AstId parse_extern_function(Parser *parser) {
    expect(parser, TOK_KW_function);
    SourceIndex token = expect(parser, TOK_ID);
    ArenaLinkedList params = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    int32_t extra = push_extra_array(&parser->ast, 1, &return_type.private_field_id);
    push_extra(&parser->ast, params);
    return add_node(AST_EXTERN_FUNCTION, (AstData) {token, params.count, extra}, &parser->ast);
}

static AstId parse_extern_mut(Parser *parser) {
    consume(parser);
    SourceIndex token = expect(parser, TOK_ID);
    AstId type_node = parse_expr(parser, PREC_NONE);
    return add_unary_ast(AST_EXTERN_MUT, token, type_node, &parser->ast);
}

static AstId parse_extern(Parser *parser) {
    expect(parser, TOK_KW_extern);

    switch (parser->lookahead.tag) {
        case TOK_KW_function:
            return parse_extern_function(parser);

        case TOK_KW_mut:
            return parse_extern_mut(parser);

        default:
            error(parser, &(ParseError) {
                .start = parser->lookahead.start,
                .end = parser->lookahead.end,
                .diag = Diagnostic(ErrorInvalidTokenAfterExtern, {
                    .provided = parser->lookahead.tag,
                }),
            });
            return null_ast;
    }
}

static AstId parse_function(Parser *parser) {
    expect(parser, TOK_KW_function);
    SourceIndex token = expect_id(parser);
    ArenaLinkedList type_parameters = parse_type_parameters(parser);
    ArenaLinkedList parameters = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    AstId body = parse_block(parser);

    int32_t extra[] = {
        type_parameters.count,
        push_extra(&parser->ast, type_parameters),
        parameters.count,
        push_extra(&parser->ast, parameters),
        return_type.private_field_id,
    };
    int32_t index = push_extra_array(&parser->ast, ArrayLength(extra), extra);
    return add_node(AST_FUNCTION, (AstData) {token, body.private_field_id, index}, &parser->ast);
}

static AstId parse_struct(Parser *parser) {
    expect(parser, TOK_KW_struct);
    SourceIndex token = expect_id(parser);
    ArenaLinkedList type_parameters = parse_type_parameters(parser);
    ArenaLinkedList fields = parse_fields(parser);
    int32_t extra[] = {
        type_parameters.count,
        push_extra(&parser->ast, type_parameters),
        push_extra(&parser->ast, fields),
    };
    int32_t index = push_extra_array(&parser->ast, ArrayLength(extra), extra);
    return add_node(AST_STRUCT, (AstData) {token, fields.count, index}, &parser->ast);
}

static AstId parse_enum(Parser *parser) {
    expect(parser, TOK_KW_enum);
    SourceIndex token = expect_id(parser);
    AstId enum_type = parse_expr(parser, PREC_NONE);
    ArenaLinkedList members = parse_enum_members(parser);
    int32_t index = push_extra_array(&parser->ast, 1, &members.count);
    push_extra(&parser->ast, members);
    return add_node(AST_ENUM, (AstData) {token, enum_type.private_field_id, index}, &parser->ast);
}

static AstId parse_newtype(Parser *parser) {
    expect(parser, TOK_KW_newtype);
    SourceIndex token = expect_id(parser);
    ArenaLinkedList type_parameters = parse_type_parameters(parser);
    expect(parser, TOK_ASSIGN);
    AstId inner = parse_expr(parser, PREC_NONE);
    int32_t index = push_extra_array(&parser->ast, 1, &type_parameters.count);
    push_extra(&parser->ast, type_parameters);
    return add_node(AST_NEWTYPE, (AstData) {token, inner.private_field_id, index}, &parser->ast);
}

static AstId parse_function_type(Parser *parser, SourceIndex token) {
    ArenaLinkedList params = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    int32_t index = push_extra_array(&parser->ast, 1, &return_type.private_field_id);
    push_extra(&parser->ast, params);
    return add_node(AST_FUNCTION_TYPE, (AstData) {token, params.count, index}, &parser->ast);
}

static AstId parse_block(Parser *parser) {
    SourceIndex block_token = expect(parser, TOK_CURLYL);
    ArenaLinkedList stmts = {0};
    while (!accept(parser, TOK_CURLYR)) {
        switch (parser->lookahead.tag) {
            case TOK_SENTINEL: {
                expect(parser, TOK_CURLYR);
                return null_ast;
            }
            case TOK_KW_let: {
                push(parser, &stmts, parse_var(parser, AST_LET));
                break;
            }
            case TOK_KW_mut: {
                push(parser, &stmts, parse_var(parser, AST_MUT));
                break;
            }
            case TOK_KW_const: {
                push(parser, &stmts, parse_var(parser, AST_CONST));
                break;
            }
            case TOK_KW_struct: {
                push(parser, &stmts, parse_struct(parser));
                break;
            }
            case TOK_KW_enum: {
                push(parser, &stmts, parse_enum(parser));
                break;
            }
            case TOK_KW_newtype: {
                push(parser, &stmts, parse_newtype(parser));
                break;
            }
            case TOK_KW_if: {
                push(parser, &stmts, parse_if(parser));
                break;
            }
            case TOK_KW_while: {
                push(parser, &stmts, parse_while(parser));
                break;
            }
            case TOK_KW_for: {
                push(parser, &stmts, parse_for(parser));
                break;
            }
            case TOK_KW_break: {
                SourceIndex token = consume(parser).start;
                AstId node = add_leaf_ast(AST_BREAK, token, &parser->ast);
                push(parser, &stmts, node);
                break;
            }
            case TOK_KW_continue: {
                SourceIndex token = consume(parser).start;
                AstId node = add_leaf_ast(AST_CONTINUE, token, &parser->ast);
                push(parser, &stmts, node);
                break;
            }
            case TOK_KW_return: {
                SourceIndex token = consume(parser).start;
                AstId value = null_ast;

                if (parser->lookahead.tag != TOK_CURLYR) {
                    value = parse_expr(parser, PREC_NONE);
                }

                AstId node = add_unary_ast(AST_RETURN, token, value, &parser->ast);
                push(parser, &stmts, node);
                break;
            }
            default: {
                AstId expr = parse_expr(parser, PREC_NONE);
                push(parser, &stmts, expr);
                break;
            }
        }
    }
    int32_t index = push_extra(&parser->ast, stmts);
    return add_node(
        AST_BLOCK,
        (AstData) {
            block_token,
            stmts.count,
            index,
        },
        &parser->ast
    );
}

static int parse_int(Parser *parser, Token token, int64_t *out) {
    int64_t result = 0;

    for (int32_t i = token.start.index; i < token.end.index; i++) {
        if (parser->lexer.source.ptr[i] == '_') {
            continue;
        }

        if (result > INT64_MAX / 10) {
            return 1;
        }

        result *= 10;
        int64_t digit = parser->lexer.source.ptr[i] - '0';

        if (result > INT64_MAX - digit) {
            return 1;
        }

        result += digit;
    }

    *out = result;
    return 0;
}

static int parse_hex_int(Parser *parser, Token token, int64_t *out) {
    union {
        int64_t value;
        uint64_t bits;
    } result;
    result.value = 0;
    int digits = 0;

    for (int32_t i = token.start.index + 2; i < token.end.index; i++) {
        if (parser->lexer.source.ptr[i] == '_') {
            continue;
        }

        if (digits >= 16) {
            return 1;
        }

        char c = parser->lexer.source.ptr[i];
        uint64_t digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        }

        result.bits <<= 4;
        result.bits |= digit & 0xF;
        digits++;
    }

    *out = result.value;
    return 0;
}

static AstId parse_unary(Parser *parser, AstTag tag, SourceIndex token) {
    AstId operand = parse_expr(parser, PREC_AS);
    return add_unary_ast(tag, token, operand, &parser->ast);
}

static AstId parse_array_type(Parser *parser, SourceIndex token) {
    AstId length = parse_expr(parser, PREC_NONE);
    expect(parser, TOK_SQUARER);
    AstId type_node = parse_expr(parser, PREC_NONE);
    return add_binary_ast(AST_ARRAY_TYPE_SUGAR, token, length, type_node, &parser->ast);
}

static AstId parse_map(Parser *parser, SourceIndex token) {
    ArenaLinkedList args = {0};

    do {
        if (parser->lookahead.tag == TOK_ROUNDR) {
            break;
        }

        SourceIndex key = expect(parser, TOK_ID);
        expect(parser, TOK_ASSIGN);
        AstId value = parse_expr(parser, PREC_NONE);
        push(parser, &args, add_unary_ast(AST_MAP_ENTRY, key, value, &parser->ast));
    } while (accept(parser, TOK_COMMA));

    expect(parser, TOK_ROUNDR);
    int32_t index = push_extra(&parser->ast, args);
    return add_node(AST_MAP, (AstData) {token, args.count, index}, &parser->ast);
}

static AstId parse_list(Parser *parser, SourceIndex token) {
    if (accept(parser, TOK_COLON)) {
        return parse_array_type(parser, token);
    }

    ArenaLinkedList args = {0};
    int32_t count = 0;

    do {
        if (parser->lookahead.tag == TOK_SQUARER) {
            break;
        }

        AstId expr = parse_expr(parser, PREC_NONE);

        if (count == 0 && accept(parser, TOK_ARROW)) {
            AstId elem_type = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_SQUARER);
            return add_binary_ast(AST_ARRAY_TYPE, token, expr, elem_type, &parser->ast);
        }

        push(parser, &args, expr);
        count++;
    } while (accept(parser, TOK_COMMA));

    expect(parser, TOK_SQUARER);
    int32_t index = push_extra(&parser->ast, args);
    return add_node(AST_LIST, (AstData) {token, args.count, index}, &parser->ast);
}

static AstId parse_prefix(Parser *parser) {
    switch (parser->lookahead.tag) {
        case TOK_ADD: {
            return parse_unary(parser, AST_PLUS, consume(parser).start);
        }
        case TOK_SUB: {
            return parse_unary(parser, AST_MINUS, consume(parser).start);
        }
        case TOK_NOT: {
            return parse_unary(parser, AST_NOT, consume(parser).start);
        }
        case TOK_AND: {
            return parse_unary(parser, AST_ADDRESS, consume(parser).start);
        }
        case TOK_MUL: {
            Token token = consume(parser);
            return parse_unary(
                parser,
                accept(parser, TOK_KW_mut)
                    ? AST_POINTER_MUT_TYPE
                    : AST_POINTER_TYPE,
                token.start
            );
        }
        case TOK_ADDRESS: {
            Token token = consume(parser);
            return parse_unary(
                parser,
                accept(parser, TOK_KW_mut)
                    ? AST_SLICE_MUT_TYPE
                    : AST_SLICE_TYPE,
                token.start
            );
        }
        case TOK_DOT: {
            consume(parser);
            return add_leaf_ast(AST_INFERRED_ACCESS, expect(parser, TOK_ID), &parser->ast);
        }
        case TOK_ROUNDL: {
            return parse_map(parser, consume(parser).start);
        }
        case TOK_SQUAREL: {
            return parse_list(parser, consume(parser).start);
        }
        case TOK_CURLYL: {
            return parse_block(parser);
        }
        case TOK_LT: {
            Token token = consume(parser);
            AstId type = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_ANGLER);
            AstId expr = parse_expr(parser, PREC_AS);
            return add_binary_ast(AST_TYPE_HINT, token.start, expr, type, &parser->ast);
        }
        case TOK_KW_function: {
            return parse_function_type(parser, consume(parser).start);
        }
        case TOK_KW_switch: {
            return parse_switch(parser, consume(parser).start);
        }
        case TOK_ID:
        case TOK_BUILTIN_ID: {
            return add_leaf_ast(AST_ID, consume(parser).start, &parser->ast);
        }
        case TOK_INT: {
            Token token = consume(parser);
            int64_t value = 0;
            if (parse_int(parser, token, &value)) {
                error(parser, &(ParseError) {
                    .start = token.start,
                    .end = token.end,
                    .diag = Diagnostic(ErrorConstIntOverflow, {0}),
                });
                return null_ast;
            }
            return add_ast_int(AST_INT, token.start, value, &parser->ast);
        }
        case TOK_HEX_INT: {
            Token token = consume(parser);
            int64_t value = 0;
            if (parse_hex_int(parser, token, &value)) {
                error(parser, &(ParseError) {
                    .start = token.start,
                    .end = token.end,
                    .diag = Diagnostic(ErrorConstIntOverflow, {0}),
                });
                return null_ast;
            }
            if (value >= 0x80000000 && value < 0x100000000) {
                value |= 0xFFFFFFFF00000000;
            }
            return add_ast_int(AST_INT, token.start, value, &parser->ast);
        }
        case TOK_FLOAT: {
            Token token = consume(parser);
            String s = substring(parser->lexer.source, token.start.index, token.end.index);
            double value = parse_float(s, parser->scratch);
            return add_ast_float(AST_FLOAT, token.start, value, &parser->ast);
        }
        case TOK_INVALID_FLOAT: {
            Token token = consume(parser);
            error(parser, &(ParseError) {
                .start = token.start,
                .end = token.end,
                .diag = Diagnostic(ErrorInvalidFloat, {0}),
            });
            return null_ast;
        }
        case TOK_CHAR: {
            Token token = consume(parser);
            return add_ast_int(
                AST_CHAR,
                token.start,
                token.end.index - token.start.index,
                &parser->ast
            );
        }
        case TOK_STRING: {
            Token token = consume(parser);
            return add_ast_int(
                AST_STRING,
                token.start,
                token.end.index - token.start.index,
                &parser->ast
            );
        }
        case TOK_KW_true: {
            return add_ast_int(AST_BOOL, consume(parser).start, 1, &parser->ast);
        }
        case TOK_KW_false: {
            return add_ast_int(AST_BOOL, consume(parser).start, 0, &parser->ast);
        }
        case TOK_KW_null: {
            return add_leaf_ast(AST_NULL, consume(parser).start, &parser->ast);
        }
        default: {
            Token token = parser->lookahead;
            error(parser, &(ParseError) {
                .start = token.start,
                .end = token.end,
                .diag = Diagnostic(ErrorExpectedExpression, {0}),
            });
            return null_ast;
        }
    }
}

static AstId parse_call(Parser *parser, SourceIndex token, AstId left) {
    ArenaLinkedList args = {0};
    do {
        if (parser->lookahead.tag == TOK_ROUNDR) {
            break;
        }

        push(parser, &args, parse_expr(parser, PREC_NONE));
    } while (accept(parser, TOK_COMMA));
    expect(parser, TOK_ROUNDR);
    int32_t index = push_extra_array(&parser->ast, 1, &left.private_field_id);
    push_extra(&parser->ast, args);
    return add_node(AST_CALL, (AstData) {token, args.count, index}, &parser->ast);
}

static AstId parse_index(Parser *parser, SourceIndex token, AstId left) {
    ArenaLinkedList args = {0};
    bool is_range = false;
    if (accept(parser, TOK_COLON)) {
        is_range = true;
    } else {
        do {
            if (parser->lookahead.tag == TOK_SQUARER) {
                break;
            }

            push(parser, &args, parse_expr(parser, PREC_NONE));

            if (accept(parser, TOK_COLON)) {
                is_range = true;
                break;
            }
        } while (accept(parser, TOK_COMMA));

        if (is_range) {
            push(parser, &args, parse_expr(parser, PREC_NONE));
        }
    }
    expect(parser, TOK_SQUARER);
    int32_t index = push_extra_array(&parser->ast, 1, &left.private_field_id);
    push_extra(&parser->ast, args);
    return add_node(is_range ? AST_SLICE : AST_INDEX, (AstData) {token, args.count, index}, &parser->ast);
}

static AstId parse_expr(Parser *parser, Precedence prec) {
    AstId left = parse_prefix(parser);
    while (!parser->lookahead.comes_after_newline && prec < get_precedence(parser->lookahead.tag)) {
        Token op = consume(parser);
        switch (op.tag) {
            case TOK_ROUNDL: {
                left = parse_call(parser, op.start, left);
                break;
            }
            case TOK_SQUAREL: {
                left = parse_index(parser, op.start, left);
                break;
            }
            case TOK_NOT: {
                left = add_unary_ast(AST_DEREF, op.start, left, &parser->ast);
                break;
            }
            case TOK_DOT: {
                left = add_unary_ast(AST_ACCESS, expect(parser, TOK_ID), left, &parser->ast);
                break;
            }
            default: {
                AstId right = parse_expr(parser, get_precedence(op.tag));
                left = add_binary_ast(bin_ast_tag(op.tag), op.start, left, right, &parser->ast);
                break;
            }
        }
    }
    return left;
}

static void parse_root(Parser *parser) {
    expect(parser, TOK_KW_module);
    SourceIndex token = parser->lookahead.start;

    if (accept(parser, TOK_KW_module)) {
        parser->internal = true;
    } else {
        expect(parser, TOK_ID);
    }

    add_unary_ast(AST_ROOT, token, null_ast, &parser->ast);
    ArenaLinkedList defs = {0};

    while (accept(parser, TOK_KW_import)) {
        AstId import = add_leaf_ast(AST_IMPORT, expect(parser, TOK_ID), &parser->ast);
        push(parser, &defs, import);
    }

    while (parser->lookahead.tag != TOK_SENTINEL) {
        SourceIndex def_token = parser->lookahead.start;
        bool is_public = accept(parser, TOK_KW_public);
        AstId def = null_ast;

        switch (parser->lookahead.tag) {
            case TOK_KW_extern:
                def = parse_extern(parser);
                break;

            case TOK_KW_function:
                def = parse_function(parser);
                break;

            case TOK_KW_struct:
                def = parse_struct(parser);
                break;

            case TOK_KW_enum:
                def = parse_enum(parser);
                break;

            case TOK_KW_newtype:
                def = parse_newtype(parser);
                break;

            case TOK_KW_const:
                def = parse_var(parser, AST_CONST);
                break;

            default:
                error(parser, &(ParseError) {
                    .start = parser->lookahead.start,
                    .end = parser->lookahead.end,
                    .diag = Diagnostic(ErrorExpectedDefinition, {0}),
                });
        }

        if (!is_ast_null(def)) {
            if (is_public) {
                def = add_unary_ast(AST_PUBLIC, def_token, def, &parser->ast);
            }

            push(parser, &defs, def);
        }
    }

    int32_t index = push_extra(&parser->ast, defs);
    parser->ast.nodes.datas[0].left = defs.count;
    parser->ast.nodes.datas[0].right = index;
}

int parse_ast(ParseInfo *info) {
    Arena scratch = new_arena(64 << 20);
    Parser parser = {0};
    parser.path = info->path;
    parser.lexer = new_lexer(info->source);
    parser.lookahead = next_valid_token(&parser);
    parser.scratch = scratch;
    parse_root(&parser);
    delete_arena(&scratch);

    if (parser.error) {
        *info->errors = parser.errors;
        return 1;
    } else {
        *info->ast = parser.ast;
        return 0;
    }
}
