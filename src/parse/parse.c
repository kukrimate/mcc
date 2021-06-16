// SPDX-License-Identifier: GPL-2.0-only

/*
 * Parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lex/token.h>
#include <pp/pp.h>
#include <target.h>
#include <err.h>
#include "type.h"
#include "parse.h"

struct ParseCtx {
    // Pre-processor context
    PpContext *pp;
    // Current token
    Token *cur;
};

// Create a parser context, reading tokens from the pre-processor
ParseCtx *parse_create(PpContext *pp)
{
    ParseCtx *ctx;

    ctx = calloc(1, sizeof *ctx);
    ctx->pp = pp;
    ctx->cur = pp_next(pp);
    return ctx;
}

// Free a parser context
void parse_free(ParseCtx *ctx)
{
    free(ctx);
}

// Read the current token
static Token *parse_cur(ParseCtx *ctx)
{
    return ctx->cur;
}

// Advance to next token
static void parse_advance(ParseCtx *ctx)
{
    ctx->cur = pp_next(ctx->pp);
}

// Like advance but also free
static void parse_eat(ParseCtx *ctx)
{
    Token *token;

    token = parse_cur(ctx);
    parse_advance(ctx);
    free_token(token);
}

// Match token and than eat if matched
static _Bool parse_match(ParseCtx *ctx, TokenType type)
{
    Token *token;

    token = parse_cur(ctx);
    if (token && token->type == type) {
        parse_advance(ctx);
        free_token(token);
        return 1;
    }
    return 0;
}

static Node *create_const(t_umax value)
{
    Node   *node;

    node = calloc(1, sizeof *node);
    node->type = ND_CONST;
    node->value = value;
    return node;
}

// Convert a preprocessing number to an integer constant
static Node *convert_int_const(Token *pp_num)
{
    char   *cur;
    t_umax value;

    cur = pp_num->data;
    value = 0;

    // Read value
    switch (*cur) {
    case '0':
        if (*++cur == 'x') {
            // Hexadecimal
            while (*++cur)
                switch (*cur) {
                case '0' ... '9':
                    value = value << 4 | (*cur - '0');
                    break;
                case 'a' ... 'f':
                    value = value << 4 | (*cur - 'a' + 0xa);
                    break;
                case 'A' ... 'F':
                    value = value << 4 | (*cur - 'A' + 0xa);
                    break;
                default:
                    goto end_value;
                }
        } else {
            // Octal
            for (; *cur; ++cur)
                switch (*cur) {
                case '0' ... '7':
                    value = value << 3 | (*cur - '0');
                    break;
                default:
                    goto end_value;
                }
        }
        break;
    default:
        // Decimal
        for (; *cur; ++cur)
            switch (*cur) {
            case '0' ... '9':
                value = value * 10 + (*cur - '0');
                break;
            default:
                goto end_value;
            }
        break;
    }
    end_value:

    return create_const(value);

// err:
//     mcc_err("Invalid character in integer constant");
}

// Convert a character constant to an integer constant node
static Node *convert_char_const(Token *char_const)
{
    char   *cur;
    t_umax value;
    Node   *node;

    // Check for empty char constant
    cur = char_const->data;
    if (!*cur)
        mcc_err("Empty character constant");

    // Convert to integer
    value = 0;
    do {
        value = value << 8 | *cur;
    } while (*++cur);

    // Create node
    node = calloc(1, sizeof *node);
    node->type = ND_CONST;
    node->value = value;
    return node;
}

// Create a node with one child
static Node *create_unary(NodeType type, Node *child1)
{
    Node *node;

    node = calloc(1, sizeof *node);
    node->type = type;
    node->child1 = child1;
    return node;
}

// Create a node with two children
static Node *create_binary(NodeType type, Node *child1, Node *child2)
{
    Node *node;

    node = calloc(1, sizeof *node);
    node->type = type;
    node->child1 = child1;
    node->child2 = child2;
    return node;
}

// Create a node with three children
static Node *create_trinary(NodeType type, Node *child1, Node *child2, Node *child3)
{
    Node *node;

    node = calloc(1, sizeof *node);
    node->type = type;
    node->child1 = child1;
    node->child2 = child2;
    node->child3 = child3;
    return node;
}

// Expression parser
static Node *p_primary(ParseCtx *ctx);
static Node *p_postfix(ParseCtx *ctx);
static Node *p_unary(ParseCtx *ctx);
static Node *p_binary(ParseCtx *ctx, Node *lhs, int min_precedence);
static Node *p_cond(ParseCtx *ctx);
static Node *p_assign(ParseCtx *ctx);
static Node *p_expression(ParseCtx *ctx);

Node *p_primary(ParseCtx *ctx)
{
    Node *node;
    Token *token;

    token = parse_cur(ctx);
    if (!token)
        goto err;

    switch (token->type) {
    case TK_PP_NUMBER:
        node = convert_int_const(token);
        parse_eat(ctx);
        break;
    case TK_CHAR_CONST:
        node = convert_char_const(token);
        parse_eat(ctx);
        break;
    case TK_LEFT_PAREN:
        parse_eat(ctx);
        node = p_expression(ctx);
        if (!parse_match(ctx, TK_RIGHT_PAREN)) {
            mcc_err("Missing )");
        }
        break;
    default:
        goto err;
    }

    return node;

err:
    mcc_err("Invalid primary expression");
}

Node *p_postfix(ParseCtx *ctx)
{
    Node *node;

    node = p_primary(ctx);

    for (;;) {
        if (parse_match(ctx, TK_PLUS_PLUS)) {
            node = create_unary(ND_POST_INC, node);
            continue;
        }

        if (parse_match(ctx, TK_MINUS_MINUS)) {
            node = create_unary(ND_POST_DEC, node);
            continue;
        }

        return node;
    }
}

Node *p_unary(ParseCtx *ctx)
{
    if (parse_match(ctx, TK_PLUS_PLUS))
        return create_unary(ND_PRE_INC, p_unary(ctx));
    if (parse_match(ctx, TK_MINUS_MINUS))
        return create_unary(ND_PRE_DEC, p_unary(ctx));
    if (parse_match(ctx, TK_AMPERSAND))
        return create_unary(ND_REF, p_unary(ctx));
    if (parse_match(ctx, TK_STAR))
        return create_unary(ND_DEREF, p_unary(ctx));
    if (parse_match(ctx, TK_PLUS)) // NOTE: we don't reflext unary + in the AST
        return p_unary(ctx);
    if (parse_match(ctx, TK_MINUS))
        return create_unary(ND_MINUS, p_unary(ctx));
    if (parse_match(ctx, TK_TILDE))
        return create_unary(ND_BIT_INV, p_unary(ctx));
    if (parse_match(ctx, TK_EXCL_MARK))
        return create_unary(ND_NOT, p_unary(ctx));
    return p_postfix(ctx);
}
static int peek_bop(ParseCtx *ctx)
{
    Token *token;

    token = parse_cur(ctx);
    if (!token)
        return -1;
    switch (token->type) {
    case TK_STAR:        return ND_MUL;
    case TK_FWD_SLASH:   return ND_DIV;
    case TK_PERCENT:     return ND_MOD;
    case TK_PLUS:        return ND_ADD;
    case TK_MINUS:       return ND_SUB;
    case TK_LEFT_SHIFT:  return ND_LSHIFT;
    case TK_RIGHT_SHIFT: return ND_RSHIFT;
    case TK_LEFT_ANGLE:  return ND_LESS;
    case TK_RIGHT_ANGLE: return ND_MORE;
    case TK_LESS_EQUAL:  return ND_LESS_EQ;
    case TK_MORE_EQUAL:  return ND_MORE_EQ;
    case TK_EQUAL_EQUAL: return ND_EQ;
    case TK_NOT_EQUAL:   return ND_NEQ;
    case TK_AMPERSAND:   return ND_AND;
    case TK_CARET:       return ND_XOR;
    case TK_VERTICAL_BAR:return ND_OR;
    case TK_LOGIC_AND:   return ND_LAND;
    case TK_LOGIC_OR:    return ND_LOR;
    default:             return -1;
    }
}

// Unlike the rest of the parser which matches the standard,
// we use an operator precedence parser for this
Node *p_binary(ParseCtx *ctx, Node *lhs, int min_precedence)
{
    // Precedence table
    static int precedences[] = {
        [ND_MUL    ] = 9, // * / %
        [ND_DIV    ] = 9,
        [ND_MOD    ] = 9,
        [ND_ADD    ] = 8, // + -
        [ND_SUB    ] = 8,
        [ND_LSHIFT ] = 7, // << >>
        [ND_RSHIFT ] = 7,
        [ND_LESS   ] = 6, // <  > <= >=
        [ND_MORE   ] = 6,
        [ND_LESS_EQ] = 6,
        [ND_MORE_EQ] = 6,
        [ND_EQ     ] = 5, // ==, !=
        [ND_NEQ    ] = 5,
        [ND_AND    ] = 4, // &
        [ND_XOR    ] = 3, // ^
        [ND_OR     ] = 2, // |
        [ND_LAND   ] = 1, // &&
        [ND_LOR    ] = 0, // ||
    };

    int op, op_next;
    Node *rhs;

    for (;;) {
        // Read opeartor
        op = peek_bop(ctx);
        if (op < 0 || precedences[op] < min_precedence)
            return lhs;
        // Eat token
        parse_eat(ctx);
        // Read RHS
        rhs = p_unary(ctx);
        // Recurse on operators with greater precedence
        for (;;) {
            op_next = peek_bop(ctx);
            if (op_next < 0 || precedences[op_next] <= precedences[op])
                break;
            rhs = p_binary(ctx, rhs, precedences[op_next]);
        }
        lhs = create_binary(op, lhs, rhs);
    }
}

Node *p_cond(ParseCtx *ctx)
{
    Node *n1, *n2;

    n1 = p_binary(ctx, p_unary(ctx), 0);

    if (!parse_match(ctx, TK_QUEST_MARK))
        return n1;

    n2 = p_expression(ctx);

    if (!parse_match(ctx, TK_COLON))
        mcc_err("Missing : from trinary conditional");

    return create_trinary(ND_COND, n1, n2, p_cond(ctx));
}

static int peek_aop(ParseCtx *ctx)
{
    Token *token;

    token = parse_cur(ctx);
    if (!token)
        return -1;
    switch (token->type) {
    case TK_EQUAL:       return ND_ASSIGN;
    case TK_MUL_EQUAL:   return ND_AS_MUL;
    case TK_DIV_EQUAL:   return ND_AS_DIV;
    case TK_REM_EQUAL:   return ND_AS_MOD;
    case TK_ADD_EQUAL:   return ND_AS_ADD;
    case TK_SUB_EQUAL:   return ND_AS_SUB;
    case TK_LSHIFT_EQUAL:return ND_AS_LSH;
    case TK_RSHIFT_EQUAL:return ND_AS_RSH;
    case TK_AND_EQUAL:   return ND_AS_AND;
    case TK_XOR_EQUAL:   return ND_AS_XOR;
    case TK_OR_EQUAL:    return ND_AS_OR;
    default:             return -1;
    }
}

Node *p_assign(ParseCtx *ctx)
{
    Node *node;
    int aop;

    // LHS
    node = p_cond(ctx);

    // Assingment
    aop = peek_aop(ctx);
    if (aop < 0)
        return node;
    parse_eat(ctx);

    return create_binary(aop, node, p_assign(ctx));
}

Node *p_expression(ParseCtx *ctx)
{
    Node *node;

    node = p_assign(ctx);
    if (!parse_match(ctx, TK_COMMA))
        return node;

    return create_binary(ND_COMMA, node, p_expression(ctx));
}

// Declaration parser
static Node *p_declaration(ParseCtx *ctx);

struct foo {
    char a;
    char b;
};

Node *p_declaration(ParseCtx *ctx)
{
    struct foo *f;
    int *p;

    f = &(struct foo) {
        .a = 'a',
        .b = 'b',
    };

    p = (int[]) {
        [6] = 6,
        [8] = 8,
    };

    printf("%p %p\n", f, p);
}

void dump_ast(Node *root);

void parse_run(ParseCtx *ctx)
{
    Node *root;

    root = p_declaration(ctx);
    root = p_declaration(ctx);
    // dump_ast(root);
}
