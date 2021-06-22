// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: constant expression evaluator
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <err.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

typedef struct {
    PpContext *pp;
    Token *cur;
} EvalCtx;

static Token *eval_next(EvalCtx *ctx, TokenType type)
{
    if (!ctx->cur)
        ctx->cur = pp_next(ctx->pp);
    if (ctx->cur && ctx->cur->type == type) {
        Token *cur = ctx->cur;
        ctx->cur = NULL;
        return cur;
    }
    return NULL;
}

static _Bool eval_match(EvalCtx *ctx, TokenType type)
{
    Token *token = eval_next(ctx, type);
    if (token) {
        free_token(token);
        return 1;
    }
    return 0;
}

static long read_number(EvalCtx *ctx, Token *pp_num)
{
    char *cur = pp_num->data;
    long value = 0;

    // Parse value
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
                    goto end;
                }
        } else {
            // Octal
            for (; *cur; ++cur)
                switch (*cur) {
                case '0' ... '7':
                    value = value << 3 | (*cur - '0');
                    break;
                default:
                    goto end;
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
                goto end;
            }
        break;
    }

end:
    for (const char **suffix = (const char *[]) { "", "u", "U", "l", "L",
            "ul", "UL", "uL", "Ul", "lu", "LU", "lU", "Lu", "ull", "ULL",
            "uLL", "Ull", "llu", "LLU", "llU", "LLu", NULL, };
            *suffix; ++suffix)
        if (!strcmp(*suffix, cur)) {
            free_token(pp_num);
            return value;
        }

    pp_err(ctx->pp, "Invalid integer constant");
}

static int octal(int ch, char **str)
{
    ch -= '0';
    switch (**str) {
    case '0' ... '7':
        ch = ch << 3 | (*(*str)++ - '0');
        break;
    default:
        goto end;
    }
    switch (**str) {
    case '0' ... '7':
        ch = ch << 3 | (*(*str)++ - '0');
        break;
    }
end:
    return ch;
}

static int hexadecimal(int ch, char **str)
{
    ch = 0;
    for (;;)
        switch (**str) {
        case '0' ... '9':
            ch = ch << 4 | (*(*str)++ - '0');
            break;
        case 'a' ... 'f':
            ch = ch << 4 | (*(*str)++ - 'a' + 0xa);
            break;
        case 'A' ... 'F':
            ch = ch << 4 | (*(*str)++ - 'A' + 0xa);
            break;
        default:
            goto end;
        }
end:
    return ch;
}

static int escseq(EvalCtx *ctx, char **str)
{
    int ch = *(*str)++;
    switch (ch) {
    case '\'':
    case '"':
    case '?':
    case '\\':
        return ch;
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'v':
        return '\v';
    case '0' ... '7':
        return octal(ch, str);
    case 'x':
        return hexadecimal(ch, str);
    default:
        pp_err(ctx->pp, "Invalid escape sequence");
    }
}

// Convert a character constant to a long
static long read_char(EvalCtx *ctx, Token *char_const)
{
    char *str = char_const->data;
    long val = 0;

    if (*str == 'L')    // Optional L prefix
        ++str;
    if (*str++ != '\'') // Must start with '
        goto err;
    if (*str == '\'')   // Must not be empty
        goto err;

    for (;;)
        switch (*str) {
        default:
            val = val << 8 | *str++;
            break;
        case '\\':
            ++str;
            val = val << 8 | escseq(ctx, &str);
            break;
        case '\'':
            free_token(char_const);
            return val;
        case 0:         // Must end with '
            goto err;
        }
err:
    pp_err(ctx->pp, "Invalid character constant");
}

static int peak_bop(EvalCtx *ctx)
{
    if (!ctx->cur)
        ctx->cur = pp_next(ctx->pp);
    if (ctx->cur) {
        switch (ctx->cur->type) {
        case TK_STAR:           break;
        case TK_FWD_SLASH:      break;
        case TK_PERCENT:        break;
        case TK_PLUS:           break;
        case TK_MINUS:          break;
        case TK_LEFT_SHIFT:     break;
        case TK_RIGHT_SHIFT:    break;
        case TK_LEFT_ANGLE:     break;
        case TK_RIGHT_ANGLE:    break;
        case TK_LESS_EQUAL:     break;
        case TK_MORE_EQUAL:     break;
        case TK_EQUAL_EQUAL:    break;
        case TK_NOT_EQUAL:      break;
        case TK_AMPERSAND:      break;
        case TK_CARET:          break;
        case TK_VERTICAL_BAR:   break;
        case TK_LOGIC_AND:      break;
        case TK_LOGIC_OR:       break;
        default:                return -1;
        }
        return ctx->cur->type;
    }
    return -1;
}

static long eval_bop(TokenType op, long lhs, long rhs)
{
    switch (op) {
    case TK_STAR:           return lhs * rhs;
    case TK_FWD_SLASH:      return lhs / rhs;
    case TK_PERCENT:        return lhs % rhs;
    case TK_PLUS:           return lhs + rhs;
    case TK_MINUS:          return lhs - rhs;
    case TK_LEFT_SHIFT:     return lhs << rhs;
    case TK_RIGHT_SHIFT:    return lhs >> rhs;
    case TK_LEFT_ANGLE:     return lhs < rhs;
    case TK_RIGHT_ANGLE:    return lhs > rhs;
    case TK_LESS_EQUAL:     return lhs <= rhs;
    case TK_MORE_EQUAL:     return lhs >= rhs;
    case TK_EQUAL_EQUAL:    return lhs == rhs;
    case TK_NOT_EQUAL:      return lhs != rhs;
    case TK_AMPERSAND:      return lhs & rhs;
    case TK_CARET:          return lhs ^ rhs;
    case TK_VERTICAL_BAR:   return lhs | rhs;
    case TK_LOGIC_AND:      return lhs && rhs;
    case TK_LOGIC_OR:       return lhs || rhs;
    default:
        abort(); // Dead code
    }
}

// Hybrid recursive descent and operator presedence parser
static long p_unary(EvalCtx *ctx);
static long p_binary(EvalCtx *ctx, long lhs, int min_precedence);
static long p_cond(EvalCtx *ctx);

long p_unary(EvalCtx *ctx)
{
    if (eval_match(ctx, TK_LEFT_PAREN)) {
        long value = p_cond(ctx);
        if (!eval_match(ctx, TK_RIGHT_PAREN))
            pp_err(ctx->pp, "Missing )");
        return value;
    }

    Token *token;
    if ((token = eval_next(ctx, TK_PP_NUMBER)))
        return read_number(ctx, token);
    if ((token = eval_next(ctx, TK_CHAR_CONST)))
        return read_char(ctx, token);

    if (eval_match(ctx, TK_IDENTIFIER))
        return 0;
    if (eval_match(ctx, TK_PLUS))
        return p_unary(ctx);
    if (eval_match(ctx, TK_MINUS))
        return -p_unary(ctx);
    if (eval_match(ctx, TK_TILDE))
        return ~p_unary(ctx);
    if (eval_match(ctx, TK_EXCL_MARK))
        return !p_unary(ctx);

    pp_err(ctx->pp, "Invalid unary expression");
}

long p_binary(EvalCtx *ctx, long lhs, int min_precedence)
{
    // Precedence table
    static int precedences[] = {
        [TK_STAR        ] = 9, // * / %
        [TK_FWD_SLASH   ] = 9,
        [TK_PERCENT     ] = 9,
        [TK_PLUS        ] = 8, // + -
        [TK_MINUS       ] = 8,
        [TK_LEFT_SHIFT  ] = 7, // << >>
        [TK_RIGHT_SHIFT ] = 7,
        [TK_LEFT_ANGLE  ] = 6, // <  > <= >=
        [TK_RIGHT_ANGLE ] = 6,
        [TK_LESS_EQUAL  ] = 6,
        [TK_MORE_EQUAL  ] = 6,
        [TK_EQUAL_EQUAL ] = 5, // ==, !=
        [TK_NOT_EQUAL   ] = 5,
        [TK_AMPERSAND   ] = 4, // &
        [TK_CARET       ] = 3, // ^
        [TK_VERTICAL_BAR] = 2, // |
        [TK_LOGIC_AND   ] = 1, // &&
        [TK_LOGIC_OR    ] = 0, // ||
    };

    for (;;) {
        // Peak for binary operator
        int op = peak_bop(ctx);
        if (op < 0 || precedences[op] < min_precedence)
            return lhs;
        // Consume operator token
        free_token(ctx->cur);
        ctx->cur = NULL;
        // Read RHS
        long rhs = p_unary(ctx);
        // Recurse on operators with greater precedence
        for (;;) {
            int next_op = peak_bop(ctx);
            if (next_op < 0 || precedences[next_op] <= precedences[op])
                break;
            rhs = p_binary(ctx, rhs, precedences[next_op]);
        }
        // Evaluate current operand
        lhs = eval_bop(op, lhs, rhs);
    }
}

long p_cond(EvalCtx *ctx)
{
    // Left side
    long l = p_binary(ctx, p_unary(ctx), 0);
    // Look for ? for conditional
    if (!eval_match(ctx, TK_QUEST_MARK))
        return l;
    // Middle
    long m = p_cond(ctx);
    // Error on missing :
    if (!eval_match(ctx, TK_COLON))
        pp_err(ctx->pp, "Missing : from trinary conditional");
    // Right
    long r = p_cond(ctx);
    // Evaluate
    return l ? m : r;
}

long eval_cexpr(PpContext *pp)
{
    EvalCtx eval_ctx = { .pp = pp, .cur = NULL };
    long value = p_cond(&eval_ctx);
    if (eval_ctx.cur || pp_next(pp)) // Make sure there are no tokens left
        pp_err(pp, "Invalid constant expression");
    return value;
}
