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

// Convert an integer constant to a long
static long read_number(PpContext *ctx, Token *pp_num)
{
    // List of allowed integer constant suffixes
    static char *allowed_suffixes[] = {
        "", "u", "U", "l", "L",
        "ul", "UL", "uL", "Ul", "lu", "LU", "lU", "Lu",
        "ull", "ULL", "uLL", "Ull", "llu", "LLU", "llU", "LLu",
        NULL
    };

    char *cur, **suf;
    long value;

    cur = pp_num->data;
    value = 0;

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
                    goto check_suf;
                }
        } else {
            // Octal
            for (; *cur; ++cur)
                switch (*cur) {
                case '0' ... '7':
                    value = value << 3 | (*cur - '0');
                    break;
                default:
                    goto check_suf;
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
                goto check_suf;
            }
        break;
    }

check_suf:
    // Check if the number has a correct suffix
    for (suf = allowed_suffixes; *suf; ++suf)
        if (!strcmp(*suf, cur))
            return value;

    // Suffix was incorrect
    pp_err(ctx, "Invalid integer constant");
}

static int octal(int ch, char **str)
{
    ch -= '0';
    // Octal constants allow 3 digits max
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
    for (;;) // Hex constants can be any length
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
            goto endloop;
        }
endloop:
    return ch;
}

static int escseq(PpContext *ctx, char **str)
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
        pp_err(ctx, "Invalid escape sequence");
    }
}

// Convert a character constant to a long
static long read_char(PpContext *ctx, Token *char_const)
{
    char *str;
    long val;

    str = char_const->data;
    val = 0;

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
            return val;
        case 0:         // Must end with '
            goto err;
        }
err:
    pp_err(ctx, "Invalid character constant");
}

// Iterator for a TokenList
typedef struct {
    TokenList *list;
    size_t i;
} TokenIterator;

#define TOKEN_IT_NEW(list) (TokenIterator) { .list = list, .i = 0 }
#define TOKEN_IT_HASCUR(it) ((it)->i < (it)->list->n)
#define TOKEN_IT_CUR(it) (it)->list->arr[(it)->i]
#define TOKEN_IT_NEXT(it) ++(it)->i

// Return next token if it matches
static Token *next_tk(TokenIterator *it, TokenType type)
{
    if (TOKEN_IT_HASCUR(it)) {
        Token *cur = TOKEN_IT_CUR(it);
        if (cur->type == type) {
            TOKEN_IT_NEXT(it);
            return cur;
        }
    }
    return NULL;
}

// Check if a token the next token binary opeartor
static int peek_bop(TokenIterator *it)
{
    if (TOKEN_IT_HASCUR(it)) {
        switch (TOKEN_IT_CUR(it)->type) {
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
        return TOKEN_IT_CUR(it)->type;
    }
    return -1;
}

// Evaluate a binary opeartor
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
static long p_unary(PpContext *ctx, TokenIterator *it);
static long p_binary(PpContext *ctx, TokenIterator *it, long lhs, int min_precedence);
static long p_cond(PpContext *ctx, TokenIterator *it);

long p_unary(PpContext *ctx, TokenIterator *it)
{
    long value;
    Token *token;

    if (next_tk(it, TK_LEFT_PAREN)) {
        value = p_cond(ctx, it);
        if (!next_tk(it, TK_RIGHT_PAREN))
            pp_err(ctx, "Missing )");
        return value;
    }
    if ((token = next_tk(it, TK_IDENTIFIER)))
        return 0;
    if ((token = next_tk(it, TK_PP_NUMBER)))
        return read_number(ctx, token);
    if ((token = next_tk(it, TK_CHAR_CONST)))
        return read_char(ctx, token);
    if (next_tk(it, TK_PLUS))
        return p_unary(ctx, it);
    if (next_tk(it, TK_MINUS))
        return -p_unary(ctx, it);
    if (next_tk(it, TK_TILDE))
        return ~p_unary(ctx, it);
    if (next_tk(it, TK_EXCL_MARK))
        return !p_unary(ctx, it);

    pp_err(ctx, "Invalid unary expression");
}

long p_binary(PpContext *ctx, TokenIterator *it, long lhs, int min_precedence)
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

    int op, op_next;
    long rhs;

    for (;;) {
        // Read operator
        op = peek_bop(it);
        if (op < 0 || precedences[op] < min_precedence)
            return lhs;
        // Move to next token
        TOKEN_IT_NEXT(it);
        // Read RHS
        rhs = p_unary(ctx, it);
        // Recurse on operators with greater precedence
        for (;;) {
            op_next = peek_bop(it);
            if (op_next < 0 || precedences[op_next] <= precedences[op])
                break;
            rhs = p_binary(ctx, it, rhs, precedences[op_next]);
        }
        // Evaluate current operand
        lhs = eval_bop(op, lhs, rhs);
    }
}

long p_cond(PpContext *ctx, TokenIterator *it)
{
    long l, m, r;

    // Left side
    l = p_binary(ctx, it, p_unary(ctx, it), 0);
    // Look for ? for conditional
    if (!next_tk(it, TK_QUEST_MARK))
        return l;
    // Middle
    m = p_cond(ctx, it);
    // Error on missing :
    if (!next_tk(it, TK_COLON))
        pp_err(ctx, "Missing : from trinary conditional");
    // Right
    r = p_cond(ctx, it);

    // Evaluate
    return l ? m : r;
}

long eval_cexpr(PpContext *ctx, TokenList *list)
{
    TokenIterator it = TOKEN_IT_NEW(list);
    long value = p_cond(ctx, &it);
    if (TOKEN_IT_HASCUR(&it)) // Make sure there are no tokens left
        pp_err(ctx, "Invalid constant expression");
    return value;
}
