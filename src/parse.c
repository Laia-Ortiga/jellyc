#include "parse.h"

#include "adt.h"
#include "ast.h"
#include "diagnostic.h"
#include "lex.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
    Vec(AstId) extra_stack;
    bool internal;
    bool error;
} Parser;

static AstId parse_block(Parser *parser);
static AstId parse_expr(Parser *parser, Precedence prec);

typedef struct {
    int32_t len;
    int32_t index;
} ExtraList;

static ExtraList new_list(Parser *parser) {
    return (ExtraList) {
        .index = parser->extra_stack.len,
    };
}

static void push_list(Parser *parser, ExtraList *list, AstId node) {
    assert(list->index == parser->extra_stack.len - list->len);
    list->len++;
    vec_push(&parser->extra_stack, node);
}

static AstId *pop_list(Parser *parser, ExtraList list) {
    assert(list.index == parser->extra_stack.len - list.len);
    parser->extra_stack.len -= list.len;
    return parser->extra_stack.ptr + list.index;
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

static ExtraList parse_type_parameters(Parser *parser) {
    ExtraList params = new_list(parser);
    if (!accept(parser, TOK_SQUAREL)) {
        return params;
    }
    while (parser->lookahead.tag != TOK_SQUARER) {
        SourceIndex token = expect_id(parser);
        AstId node = ast_push_tag(&parser->ast, AST_ID, (AstLeaf) {
            .token = token,
        });
        push_list(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_SQUARER);
    return params;
}

static ExtraList parse_parameters(Parser *parser) {
    expect(parser, TOK_ROUNDL);
    ExtraList params = new_list(parser);
    while (parser->lookahead.tag != TOK_ROUNDR) {
        SourceIndex token = expect_id(parser);
        AstId type_node = parse_expr(parser, PREC_NONE);
        AstId node = ast_push(&parser->ast, (AstParam) {
            .token = token,
            .type = type_node,
        });
        push_list(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_ROUNDR);
    return params;
}

static ExtraList parse_fields(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ExtraList params = new_list(parser);
    while (parser->lookahead.tag != TOK_CURLYR) {
        SourceIndex token = expect(parser, TOK_ID);
        AstId type_node = parse_expr(parser, PREC_NONE);
        AstId node = ast_push(&parser->ast, (AstParam) {
            .token = token,
            .type = type_node,
        });
        push_list(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_CURLYR);
    return params;
}

static ExtraList parse_enum_members(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ExtraList params = new_list(parser);
    while (parser->lookahead.tag != TOK_CURLYR) {
        SourceIndex token = expect(parser, TOK_ID);
        AstId node = ast_push_tag(&parser->ast, AST_ID, (AstLeaf) {
            .token = token,
        });
        push_list(parser, &params, node);
        if (!accept(parser, TOK_COMMA)) {
            break;
        }
    }
    expect(parser, TOK_CURLYR);
    return params;
}

static AstId parse_let(Parser *parser, AstTag tag) {
    consume(parser);
    SourceIndex token = expect_id(parser);
    expect(parser, TOK_ASSIGN);
    AstId init = parse_expr(parser, PREC_NONE);
    return ast_push_tag(&parser->ast, tag, (AstLet) {
        .token = token,
        .init = init,
    });
}

static AstId parse_const(Parser *parser) {
    consume(parser);
    SourceIndex token = expect_id(parser);
    expect(parser, TOK_ASSIGN);
    AstId init = parse_expr(parser, PREC_NONE);
    return ast_push(&parser->ast, (AstConst) {
        .token = token,
        .init = init,
    });
}

static AstId parse_if(Parser *parser) {
    SourceIndex token = consume(parser).start;
    AstId cond = parse_expr(parser, PREC_NONE);
    AstId true_block = parse_block(parser);

    AstId false_block = null_ast;
    if (accept(parser, TOK_KW_else)) {
        false_block = parse_block(parser);
    }

    return ast_push(&parser->ast, (AstIf) {
        .token = token,
        .condition = cond,
        .true_block = true_block,
        .false_block = false_block,
    });
}

static AstId parse_while(Parser *parser) {
    SourceIndex token = consume(parser).start;
    AstId cond = parse_expr(parser, PREC_NONE);
    AstId block = parse_block(parser);
    return ast_push(&parser->ast, (AstWhile) {
        .token = token,
        .condition = cond,
        .block = block,
    });
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
    return ast_push(&parser->ast, (AstFor) {
        .token = token,
        .init = init,
        .condition = cond,
        .next = next,
        .block = block,
    });
}

static ExtraList parse_switch_cases(Parser *parser) {
    expect(parser, TOK_CURLYL);
    ExtraList cases = new_list(parser);
    while (!accept(parser, TOK_SENTINEL) && !accept(parser, TOK_CURLYR)) {
        SourceIndex token = parser->lookahead.start;
        if (parser->lookahead.tag == TOK_KW_else) {
            // default case
            consume(parser);
            expect(parser, TOK_ARROW);
            AstId value = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_COMMA);
            AstId node = ast_push(&parser->ast, (AstSwitchCase) {
                .token = token,
                .pattern = null_ast,
                .value = value,
            });
            push_list(parser, &cases, node);
        } else {
            AstId cond = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_ARROW);
            AstId value = parse_expr(parser, PREC_NONE);
            expect(parser,TOK_COMMA);
            AstId node = ast_push(&parser->ast, (AstSwitchCase) {
                .token = token,
                .pattern = cond,
                .value = value,
            });
            push_list(parser, &cases, node);
        }
    }
    return cases;
}

static AstId parse_switch(Parser *parser, SourceIndex token) {
    AstId cond = parse_expr(parser, PREC_NONE);
    ExtraList branches = parse_switch_cases(parser);
    return ast_push(&parser->ast, (AstSwitch) {
        .token = token,
        .condition = cond,
        .branches = {branches.len, pop_list(parser, branches)},
    });
}

static AstId parse_extern_function(Parser *parser) {
    expect(parser, TOK_KW_function);
    SourceIndex token = expect(parser, TOK_ID);
    ExtraList params = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    return ast_push(&parser->ast, (AstExternFunction) {
        .token = token,
        .params = {params.len, pop_list(parser, params)},
        .ret = return_type,
    });
}

static AstId parse_extern_mut(Parser *parser) {
    consume(parser);
    SourceIndex token = expect(parser, TOK_ID);
    AstId type_node = parse_expr(parser, PREC_NONE);
    return ast_push(&parser->ast, (AstExternVar) {
        .token = token,
        .type = type_node,
    });
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
    ExtraList type_parameters = parse_type_parameters(parser);
    ExtraList parameters = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    AstId body = parse_block(parser);
    AstId *parameters_ptr = pop_list(parser, parameters);
    AstId *type_parameters_ptr = pop_list(parser, type_parameters);

    return ast_push(&parser->ast, (AstFunction) {
        .token = token,
        .type_params = {type_parameters.len, type_parameters_ptr},
        .params = {parameters.len, parameters_ptr},
        .ret = return_type,
        .body = body,
    });
}

static AstId parse_struct(Parser *parser) {
    expect(parser, TOK_KW_struct);
    SourceIndex token = expect_id(parser);
    ExtraList type_parameters = parse_type_parameters(parser);
    ExtraList fields = parse_fields(parser);
    AstId *fields_ptr = pop_list(parser, fields);
    AstId *type_parameters_ptr = pop_list(parser, type_parameters);
    return ast_push(&parser->ast, (AstStruct) {
        .token = token,
        .type_params = {type_parameters.len, type_parameters_ptr},
        .fields = {fields.len, fields_ptr},
    });
}

static AstId parse_enum(Parser *parser) {
    expect(parser, TOK_KW_enum);
    SourceIndex token = expect_id(parser);
    AstId enum_type = parse_expr(parser, PREC_NONE);
    ExtraList members = parse_enum_members(parser);
    return ast_push(&parser->ast, (AstEnum) {
        .token = token,
        .repr = enum_type,
        .members = {members.len, pop_list(parser, members)},
    });
}

static AstId parse_newtype(Parser *parser) {
    expect(parser, TOK_KW_newtype);
    SourceIndex token = expect_id(parser);
    ExtraList type_parameters = parse_type_parameters(parser);
    expect(parser, TOK_ASSIGN);
    AstId inner = parse_expr(parser, PREC_NONE);
    return ast_push(&parser->ast, (AstNewtype) {
        .token = token,
        .type_params = {type_parameters.len, pop_list(parser, type_parameters)},
        .type = inner,
    });
}

static AstId parse_function_type(Parser *parser, SourceIndex token) {
    ExtraList params = parse_parameters(parser);

    AstId return_type = null_ast;
    if (accept(parser, TOK_ARROW)) {
        return_type = parse_expr(parser, PREC_NONE);
    }

    return ast_push(&parser->ast, (AstFunctionType) {
        .token = token,
        .params = {params.len, pop_list(parser, params)},
        .ret = return_type,
    });
}

static AstId parse_block(Parser *parser) {
    SourceIndex block_token = expect(parser, TOK_CURLYL);
    ExtraList stmts = new_list(parser);
    while (!accept(parser, TOK_CURLYR)) {
        switch (parser->lookahead.tag) {
            case TOK_SENTINEL: {
                expect(parser, TOK_CURLYR);
                goto end;
            }
            case TOK_KW_let: {
                push_list(parser, &stmts, parse_let(parser, AST_LET));
                break;
            }
            case TOK_KW_mut: {
                push_list(parser, &stmts, parse_let(parser, AST_MUT));
                break;
            }
            case TOK_KW_const: {
                push_list(parser, &stmts, parse_const(parser));
                break;
            }
            case TOK_KW_struct: {
                push_list(parser, &stmts, parse_struct(parser));
                break;
            }
            case TOK_KW_enum: {
                push_list(parser, &stmts, parse_enum(parser));
                break;
            }
            case TOK_KW_newtype: {
                push_list(parser, &stmts, parse_newtype(parser));
                break;
            }
            case TOK_KW_if: {
                push_list(parser, &stmts, parse_if(parser));
                break;
            }
            case TOK_KW_while: {
                push_list(parser, &stmts, parse_while(parser));
                break;
            }
            case TOK_KW_for: {
                push_list(parser, &stmts, parse_for(parser));
                break;
            }
            case TOK_KW_break: {
                SourceIndex token = consume(parser).start;
                AstId node = ast_push(&parser->ast, (AstBreak) {
                    .token = token,
                });
                push_list(parser, &stmts, node);
                break;
            }
            case TOK_KW_continue: {
                SourceIndex token = consume(parser).start;
                AstId node = ast_push(&parser->ast, (AstContinue) {
                    .token = token,
                });
                push_list(parser, &stmts, node);
                break;
            }
            case TOK_KW_return: {
                SourceIndex token = consume(parser).start;
                AstId value = null_ast;

                if (parser->lookahead.tag != TOK_CURLYR) {
                    value = parse_expr(parser, PREC_NONE);
                }

                AstId node = ast_push(&parser->ast, (AstReturn) {
                    .token = token,
                    .value = value,
                });
                push_list(parser, &stmts, node);
                break;
            }
            default: {
                AstId expr = parse_expr(parser, PREC_NONE);
                push_list(parser, &stmts, expr);
                break;
            }
        }
    }
end:
    return ast_push(&parser->ast, (AstBlock) {
        .token = block_token,
        .stmts = {stmts.len, pop_list(parser, stmts)},
    });
}

static AstId parse_unary(Parser *parser, AstTag tag, SourceIndex token) {
    AstId operand = parse_expr(parser, PREC_AS);
    return ast_push_tag(&parser->ast, tag, (AstUnary) {
        .token = token,
        .a = operand,
    });
}

static AstId parse_array_type(Parser *parser, SourceIndex token) {
    AstId length = parse_expr(parser, PREC_NONE);
    expect(parser, TOK_SQUARER);
    AstId type_node = parse_expr(parser, PREC_NONE);
    return ast_push(&parser->ast, (AstArrayTypeSugar) {
        .token = token,
        .length = length,
        .elem = type_node,
    });
}

static AstId parse_map(Parser *parser, SourceIndex token) {
    ExtraList args = new_list(parser);

    do {
        if (parser->lookahead.tag == TOK_ROUNDR) {
            break;
        }

        SourceIndex key = expect(parser, TOK_ID);
        expect(parser, TOK_ASSIGN);
        AstId value = parse_expr(parser, PREC_NONE);
        push_list(parser, &args, ast_push(&parser->ast, (AstMapEntry) {
            .token = key,
            .value = value,
        }));
    } while (accept(parser, TOK_COMMA));

    expect(parser, TOK_ROUNDR);
    return ast_push(&parser->ast, (AstMap) {
        .token = token,
        .entries = {args.len, pop_list(parser, args)},
    });
}

static AstId parse_list(Parser *parser, SourceIndex token) {
    if (accept(parser, TOK_COLON)) {
        return parse_array_type(parser, token);
    }

    ExtraList args = new_list(parser);
    int32_t count = 0;

    do {
        if (parser->lookahead.tag == TOK_SQUARER) {
            break;
        }

        AstId expr = parse_expr(parser, PREC_NONE);

        if (count == 0 && accept(parser, TOK_ARROW)) {
            AstId elem_type = parse_expr(parser, PREC_NONE);
            expect(parser, TOK_SQUARER);
            return ast_push(&parser->ast, (AstArrayType) {
                .token = token,
                .index = expr,
                .elem = elem_type,
            });
        }

        push_list(parser, &args, expr);
        count++;
    } while (accept(parser, TOK_COMMA));

    expect(parser, TOK_SQUARER);
    return ast_push(&parser->ast, (AstList) {
        .token = token,
        .elems = {args.len, pop_list(parser, args)},
    });
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
                    ? AST_MUT_PTR_TYPE
                    : AST_PTR_TYPE,
                token.start
            );
        }
        case TOK_ADDRESS: {
            Token token = consume(parser);
            return parse_unary(
                parser,
                accept(parser, TOK_KW_mut)
                    ? AST_MUT_SLICE_TYPE
                    : AST_SLICE_TYPE,
                token.start
            );
        }
        case TOK_DOT: {
            consume(parser);
            return ast_push(&parser->ast, (AstInferredAccess) {
                .token = expect(parser, TOK_ID),
            });
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
            AstId type = parse_expr(parser, PREC_AS);
            expect(parser, TOK_GT);
            AstId expr = parse_expr(parser, PREC_AS);
            return ast_push(&parser->ast, (AstTypeHint) {
                .token = token.start,
                .type = type,
                .value = expr,
            });
        }
        case TOK_KW_function: {
            return parse_function_type(parser, consume(parser).start);
        }
        case TOK_KW_switch: {
            return parse_switch(parser, consume(parser).start);
        }
        case TOK_ID:
        case TOK_BUILTIN_ID: {
            return ast_push_tag(&parser->ast, AST_ID, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_INT:
        case TOK_HEX_INT: {
            return ast_push_tag(&parser->ast, AST_INT, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_FLOAT: {
            return ast_push_tag(&parser->ast, AST_FLOAT, (AstLeaf) {
                .token = consume(parser).start,
            });
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
            return ast_push_tag(&parser->ast, AST_CHAR, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_STRING: {
            return ast_push_tag(&parser->ast, AST_STRING, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_KW_true: {
            return ast_push_tag(&parser->ast, AST_TRUE, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_KW_false: {
            return ast_push_tag(&parser->ast, AST_FALSE, (AstLeaf) {
                .token = consume(parser).start,
            });
        }
        case TOK_KW_null: {
            return ast_push_tag(&parser->ast, AST_NULL, (AstLeaf) {
                .token = consume(parser).start,
            });
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
    ExtraList args = new_list(parser);
    do {
        if (parser->lookahead.tag == TOK_ROUNDR) {
            break;
        }

        push_list(parser, &args, parse_expr(parser, PREC_NONE));
    } while (accept(parser, TOK_COMMA));
    expect(parser, TOK_ROUNDR);
    return ast_push_tag(&parser->ast, AST_CALL, (AstCall) {
        .token = token,
        .a = left,
        .args = {args.len, pop_list(parser, args)},
    });
}

static AstId parse_index(Parser *parser, SourceIndex token, AstId left) {
    ExtraList args = new_list(parser);
    bool is_range = false;
    if (accept(parser, TOK_COLON)) {
        is_range = true;
        push_list(parser, &args, null_ast);
        if (parser->lookahead.tag != TOK_SQUARER) {
            push_list(parser, &args, parse_expr(parser, PREC_NONE));
        }
    } else {
        do {
            if (parser->lookahead.tag == TOK_SQUARER) {
                break;
            }

            push_list(parser, &args, parse_expr(parser, PREC_NONE));

            if (accept(parser, TOK_COLON)) {
                is_range = true;
                break;
            }
        } while (accept(parser, TOK_COMMA));

        if (is_range) {
            push_list(parser, &args, parse_expr(parser, PREC_NONE));
        }
    }
    expect(parser, TOK_SQUARER);
    return ast_push_tag(&parser->ast, is_range ? AST_SLICE : AST_INDEX, (AstCall) {
        .token = token,
        .a = left,
        .args = {args.len, pop_list(parser, args)},
    });
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
                left = ast_push_tag(&parser->ast, AST_DEREF, (AstUnary) {
                    .token = op.start,
                    .a = left,
                });
                break;
            }
            case TOK_DOT: {
                left = ast_push(&parser->ast, (AstAccess) {
                    .token = expect(parser, TOK_ID),
                    .s = left,
                });
                break;
            }
            default: {
                AstId right = parse_expr(parser, get_precedence(op.tag));
                left = ast_push_tag(&parser->ast, bin_ast_tag(op.tag), (AstBinary) {
                    .token = op.start,
                    .a = left,
                    .b = right,
                });
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

    ast_push(&parser->ast, (AstRoot) {
        .token = token,
    });
    ExtraList defs = new_list(parser);

    while (accept(parser, TOK_KW_import)) {
        AstId import = ast_push(&parser->ast, (AstImport) {
            .token = expect(parser, TOK_ID),
        });
        push_list(parser, &defs, import);
    }

    while (parser->lookahead.tag != TOK_SENTINEL) {
        SourceIndex def_token = parser->lookahead.start;
        bool is_public = accept(parser, TOK_KW_public);
        AstId def = null_ast;

        switch (parser->lookahead.tag) {
            case TOK_KW_extern: {
                def = parse_extern(parser);
                break;
            }
            case TOK_KW_function: {
                def = parse_function(parser);
                break;
            }
            case TOK_KW_struct: {
                def = parse_struct(parser);
                break;
            }
            case TOK_KW_enum: {
                def = parse_enum(parser);
                break;
            }
            case TOK_KW_newtype: {
                def = parse_newtype(parser);
                break;
            }
            case TOK_KW_const: {
                def = parse_const(parser);
                break;
            }
            default: {
                error(parser, &(ParseError) {
                    .start = parser->lookahead.start,
                    .end = parser->lookahead.end,
                    .diag = Diagnostic(ErrorExpectedDefinition, {0}),
                });
            }
        }

        if (!is_ast_null(def)) {
            if (is_public) {
                def = ast_push(&parser->ast, (AstPublic) {
                    .token = def_token,
                    .def = def,
                });
            }

            push_list(parser, &defs, def);
        }
    }

    nth(parser->ast.nodes.data_table, null_ast).b = defs.len;
    nth(parser->ast.nodes.data_table, null_ast).c = parser->ast.extra.len;
    AstId *extra = (AstId *) vec_grow(&parser->ast.extra, defs.len);
    memcpy(extra, pop_list(parser, defs), defs.len * sizeof(AstId));
    assert(parser->extra_stack.len == 0);
}

int parse_ast(ParseInfo *info) {
    Parser parser = {0};
    parser.path = info->path;
    parser.lexer = new_lexer(info->source);
    parser.lookahead = next_valid_token(&parser);
    parse_root(&parser);
    free(parser.extra_stack.ptr);

    if (parser.error) {
        *info->errors = parser.errors;
        return 1;
    } else {
        *info->ast = parser.ast;
        return 0;
    }
}
