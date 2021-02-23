/*
 * Simple constant expression evaluator, this builds no AST thus pre-processor
 * constant expressions are evaluated quicker, also this removes the circular
 * depedency between the parser and the pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "token.h"
#include "cexpr.h"

// Convert an integer constant to a long
static long read_number(Token *pp_num)
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
                value = value * 10 | (*cur - '0');
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
    mcc_err("Invalid integer constant");
}

// Convert a character constant to a long
static long read_char(Token *char_const)
{
    char *cur;
    long value;

    cur = char_const->data;
    value = 0;

    // Check for empty char constant
    if (!*cur)
        mcc_err("Empty character constant");

    // Read all characters
    for (; *cur; ++cur)
        value = value << 8 | *cur;

    return value;
}

// Return next token if it matches
static Token *next_tk(Token **tail, TokenType type)
{
    Token *tmp;

    if (!*tail || (*tail)->type != type)
        return NULL;

    tmp = *tail;
    *tail = tmp->next;
    return tmp;
}

// See if the tail of the list matche
static _Bool match_tk(Token **tail, TokenType type)
{
    if (!*tail || (*tail)->type != type)
        return 0;
    *tail = (*tail)->next;
    return 1;
}

// Check if a token is a binary opeartor
static int peek_bop(Token **tail)
{
    if (!*tail)
        return -1;
    switch ((*tail)->type) {
    default:                return -1;
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
    }
    return (*tail)->type;
}

// Evaluate a binary opeartor
static long eval_bop(int op, long lhs, long rhs)
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
    }
    abort(); // NOTE: can't happen
}

// Hybrid recursive descent and operator presedence parser
static long p_unary(Token **tail);
static long p_binary(Token **tail, long lhs, int min_precedence);
static long p_cond(Token **tail);

long p_unary(Token **tail)
{
    long value;
    Token *token;

    if (match_tk(tail, TK_LEFT_PAREN)) {
        value = p_cond(tail);
        if (!match_tk(tail, TK_RIGHT_PAREN))
            mcc_err("Missing )");
        return value;
    }
    if ((token = next_tk(tail, TK_PP_NUMBER)))
        return read_number(token);
    if ((token = next_tk(tail, TK_CHAR_CONST)))
        return read_char(token);
    if (match_tk(tail, TK_PLUS))
        return p_unary(tail);
    if (match_tk(tail, TK_MINUS))
        return -p_unary(tail);
    if (match_tk(tail, TK_TILDE))
        return ~p_unary(tail);
    if (match_tk(tail, TK_EXCL_MARK))
        return !p_unary(tail);

    mcc_err("Invalid unary expression");
}

long p_binary(Token **tail, long lhs, int min_precedence)
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
        op = peek_bop(tail);
        if (op < 0 || precedences[op] < min_precedence)
            return lhs;
        // Move to next token
        *tail = (*tail)->next;
        // Read RHS
        rhs = p_unary(tail);
        // Recurse on operators with greater precedence
        for (;;) {
            op_next = peek_bop(tail);
            if (op_next < 0 || precedences[op_next] <= precedences[op])
                break;
            rhs = p_binary(tail, rhs, precedences[op_next]);
        }
        // Evaluate current operand
        lhs = eval_bop(op, lhs, rhs);
    }
}

long p_cond(Token **tail)
{
    long l, m, r;

    // Left side
    l = p_binary(tail, p_unary(tail), 0);
    // Look for ? for conditional
    if (!match_tk(tail, TK_QUEST_MARK))
        return l;
    // Middle
    m = p_cond(tail);
    // Error on missing :
    if (!match_tk(tail, TK_COLON))
        mcc_err("Missing : from trinary conditional");
    // Right
    r = p_cond(tail);

    // Evaluate
    return l ? m : r;
}

long eval_cexpr(Token *head)
{
    long value;

    value = p_cond(&head);
    // Make sure there are no tokens left
    if (head)
        mcc_err("Invalid constant expression");
    return value;
}
