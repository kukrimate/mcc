/*
 * Parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "target.h"
#include "pp/token.h"
#include "pp/pp.h"
#include "parse.h"

typedef enum {
    SRC_PP,   // Preprocessor
    SRC_LIST, // List
} ParseSrc;

struct ParseCtx {
    ParseSrc src;
    PpContext *pp_ctx;
    Token *head;
};

// Create a parser context, reading tokens from the pre-processor
ParseCtx *parse_open(PpContext *pp_ctx)
{
    ParseCtx *parse_ctx;

    parse_ctx = calloc(1, sizeof *parse_ctx);
    parse_ctx->src = SRC_PP;
    parse_ctx->pp_ctx = pp_ctx;
    parse_ctx->head = NULL;
    return parse_ctx;
}

// Create a parser context, reading tokens from a list
ParseCtx *parse_open_list(Token *head)
{
    ParseCtx *parse_ctx;

    parse_ctx = calloc(1, sizeof *parse_ctx);
    parse_ctx->src = SRC_LIST;
    parse_ctx->head = head;
    return parse_ctx;
}

// Free a parser context
void parse_free(ParseCtx *parse_ctx)
{
    free(parse_ctx);
}

// Read the next token
static Token *parse_read(ParseCtx *parse_ctx)
{
    Token *tmp;

    switch (parse_ctx->src) {
    case SRC_PP:
        if (parse_ctx->head) {
            tmp = parse_ctx->head;
            parse_ctx->head = NULL;
        } else {
            tmp = pp_proc(parse_ctx->pp_ctx);
        }
        break;
    case SRC_LIST:
        tmp = parse_ctx->head;
        if (tmp)
            parse_ctx->head = parse_ctx->head->next;
        break;
    }
    return tmp;
}

// Peek at the next token
static Token *parse_peek(ParseCtx *parse_ctx)
{
    // Return token if we already got it
    if (parse_ctx->head)
        return parse_ctx->head;

    // If we are reading from the pre-processor fill in token
    if (parse_ctx->src == SRC_PP)
        parse_ctx->head = pp_proc(parse_ctx->pp_ctx);

    // Return token or NULL
    return parse_ctx->head;
}

static _Bool parse_match(ParseCtx *parse_ctx, TokenType type)
{
    Token *token;

    token = parse_peek(parse_ctx);
    if (token && token->type == type) {
        free_token(parse_read(parse_ctx));
        return 1;
    }
    return 0;
}

static Node *create_cosnt(t_umax value)
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
                value = value * 10 | (*cur - '0');
                break;
            default:
                goto end_value;
            }
        break;
    }
end_value:

    // Determine type
    // if (!strcmp(cur, "ull") || !strcmp(cur, "ULL") || !strcmp(cur, "llu") ||
    //     !strcmp(cur, "LLU") || !strcmp(cur, "LLu") || !strcmp(cur, "llU") ||
    //     !strcmp(cur, "uLL") || !strcmp(cur, "Ull"))


    // Check for unexpected chars
    if (*cur)
        goto err;

    return create_cosnt(value);
err:
    mcc_err("Invalid character in integer constant");
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

// Print an AST
static void dump_ast(Node *root)
{

}

// Recursive descent parser
static Node *p_primary(ParseCtx *parse_ctx);
static Node *p_postfix(ParseCtx *parse_ctx);
static Node *p_unary(ParseCtx *parse_ctx);
static Node *p_binary(ParseCtx *parse_ctx, Node *lhs, int min_precedence);
static Node *p_cond(ParseCtx *parse_ctx);
static Node *p_assign(ParseCtx *parse_ctx);
static Node *p_expression(ParseCtx *parse_ctx);

Node *p_primary(ParseCtx *parse_ctx)
{
    Node *node;
    Token *token;

    token = parse_read(parse_ctx);
    if (!token)
        goto err;

    switch (token->type) {
    case TK_PP_NUMBER:
        node = convert_int_const(token);
        break;
    case TK_CHAR_CONST:
        node = convert_char_const(token);
        break;
    case TK_LEFT_PAREN:
        node = p_expression(parse_ctx);
        if (!parse_match(parse_ctx, TK_RIGHT_PAREN)) {
            mcc_err("Missing )");
        }
        break;
    default:
        goto err;
    }

    free_token(token);
    return node;
err:
    abort();
    mcc_err("Invalid primary expression");
}

Node *p_postfix(ParseCtx *parse_ctx)
{
    Node *node;

    node = p_primary(parse_ctx);

    for (;;) {
        if (parse_match(parse_ctx, TK_PLUS_PLUS)) {
            node = create_unary(ND_POST_INC, node);
            continue;
        }

        if (parse_match(parse_ctx, TK_MINUS_MINUS)) {
            node = create_unary(ND_POST_DEC, node);
            continue;
        }

        return node;
    }
}

Node *p_unary(ParseCtx *parse_ctx)
{
    Node *node;

    node = p_postfix(parse_ctx);

    for (;;) {
        if (parse_match(parse_ctx, TK_PLUS_PLUS)) {
            node = create_unary(ND_PRE_INC, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_MINUS_MINUS)) {
            node = create_unary(ND_PRE_DEC, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_AMPERSAND)) {
            node = create_unary(ND_REF, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_STAR)) {
            node = create_unary(ND_DEREF, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_PLUS)) {
            node = create_unary(ND_POS, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_MINUS)) {
            node = create_unary(ND_NEG, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_TILDE)) {
            node = create_unary(ND_INV, node);
            continue;
        }
        if (parse_match(parse_ctx, TK_EXCL_MARK)) {
            node = create_unary(ND_NOT, node);
            continue;
        }
        return node;
    }
}

static int peek_bop(ParseCtx *parse_ctx)
{
    switch (parse_peek(parse_ctx)->type) {
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
Node *p_binary(ParseCtx *parse_ctx, Node *lhs, int min_precedence)
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
        op = peek_bop(parse_ctx);
        if (op < 0 || precedences[op] < min_precedence)
            return lhs;
        // Eat token
        free_token(parse_read(parse_ctx));
        // Read RHS
        rhs = p_unary(parse_ctx);
        // Recurse on operators with greater precedence
        for (;;) {
            op_next = peek_bop(parse_ctx);
            if (op_next < 0 || precedences[op_next] <= precedences[op])
                break;
            rhs = p_binary(parse_ctx, rhs, precedences[op_next]);
        }
        lhs = create_binary(op, lhs, rhs);
    }
}

Node *p_cond(ParseCtx *parse_ctx)
{
    Node *n1, *n2;

    n1 = p_binary(parse_ctx, p_unary(parse_ctx), 0);

    if (!parse_match(parse_ctx, TK_QUEST_MARK))
        return n1;

    n2 = p_expression(parse_ctx);

    if (!parse_match(parse_ctx, TK_COLON))
        mcc_err("Missing : from trinary conditional");

    return create_trinary(ND_COND, n1, n2, p_cond(parse_ctx));
}

static int peek_aop(ParseCtx *parse_ctx)
{
    switch (parse_peek(parse_ctx)->type) {
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

Node *p_assign(ParseCtx *parse_ctx)
{
    Node *node;
    int aop;

    node = p_cond(parse_ctx);
    aop = peek_aop(parse_ctx);
    if (aop < 0)
        return node;

    return create_binary(aop, node, p_assign(parse_ctx));
}

Node *p_expression(ParseCtx *parse_ctx)
{
    Node *node;

    node = p_assign(parse_ctx);
    if (!parse_match(parse_ctx, TK_COMMA))
        return node;

    return create_binary(TK_COMMA, node, p_assign(parse_ctx));
}

void parse_run(ParseCtx *parse_ctx)
{
    Node *root;

    root = p_expression(parse_ctx);
}
