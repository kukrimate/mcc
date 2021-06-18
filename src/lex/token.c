// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor token utility functions
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <err.h>
#include "token.h"
#include "lex.h"

Token *create_token(TokenType type, TokenFlags flags, char *data)
{
    Token *token;

    token = calloc(1, sizeof *token);
    token->refcnt = 1;
    token->type = type;
    token->flags = flags;
    token->data = data;
    return token;
}

Token *ref_token(Token *token)
{
    ++token->refcnt;
    return token;
}

void free_token(Token *token)
{
    if (!--token->refcnt) {
        if (token->data)
            free(token->data);
        free(token);
    }
}

static char *token_str[] = {
    [TK_NEW_LINE     ] = "\n",
    [TK_LEFT_SQUARE  ] = "[",
    [TK_RIGHT_SQUARE ] = "]",
    [TK_LEFT_PAREN   ] = "(",
    [TK_RIGHT_PAREN  ] = ")",
    [TK_LEFT_CURLY   ] = "{",
    [TK_RIGHT_CURLY  ] = "}",
    [TK_MEMBER       ] = ".",
    [TK_DEREF_MEMBER ] = "->",
    [TK_PLUS_PLUS    ] = "++",
    [TK_MINUS_MINUS  ] = "--",
    [TK_AMPERSAND    ] = "&",
    [TK_STAR         ] = "*",
    [TK_PLUS         ] = "+",
    [TK_MINUS        ] = "-",
    [TK_TILDE        ] = "~",
    [TK_EXCL_MARK    ] = "!",
    [TK_FWD_SLASH    ] = "/",
    [TK_PERCENT      ] = "%",
    [TK_LEFT_SHIFT   ] = "<<",
    [TK_RIGHT_SHIFT  ] = ">>",
    [TK_LEFT_ANGLE   ] = "<",
    [TK_RIGHT_ANGLE  ] = ">",
    [TK_LESS_EQUAL   ] = "<=",
    [TK_MORE_EQUAL   ] = ">=",
    [TK_EQUAL_EQUAL  ] = "==",
    [TK_NOT_EQUAL    ] = "!=",
    [TK_CARET        ] = "^",
    [TK_VERTICAL_BAR ] = "|",
    [TK_LOGIC_AND    ] = "&&",
    [TK_LOGIC_OR     ] = "||",
    [TK_QUEST_MARK   ] = "?",
    [TK_COLON        ] = ":",
    [TK_SEMICOLON    ] = ";",
    [TK_VARARGS      ] = "...",
    [TK_EQUAL        ] = "=",
    [TK_MUL_EQUAL    ] = "*=",
    [TK_DIV_EQUAL    ] = "/=",
    [TK_REM_EQUAL    ] = "%=",
    [TK_ADD_EQUAL    ] = "+=",
    [TK_SUB_EQUAL    ] = "-=",
    [TK_LSHIFT_EQUAL ] = "<<=",
    [TK_RSHIFT_EQUAL ] = ">>=",
    [TK_AND_EQUAL    ] = "&=",
    [TK_XOR_EQUAL    ] = "^=",
    [TK_OR_EQUAL     ] = "|=",
    [TK_COMMA        ] = ",",
    [TK_HASH         ] = "#",
    [TK_HASH_HASH    ] = "##",
};

static const char *token_spelling(Token *token)
{
    switch (token->type) {
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
    case TK_CHAR_CONST:
    case TK_STRING_LIT:
    case TK_OTHER:
        return token->data;
    default:
        return token_str[token->type];
    }
}

void output_token(Token *token)
{
    /*if (token->flags.lwhite)*/
        putchar(' ');
    fputs(token_spelling(token), stdout);
}

void token_list_freeall(TokenList *list)
{
    for (size_t i = 0; i < list->n; ++i)
        free_token(list->arr[i]);
    token_list_free(list);
}

void token_list_refxtend(TokenList *list, TokenList *other)
{
    for (size_t i = 0; i < other->n; ++i)
        token_list_add(list, ref_token(other->arr[i]));
}

char *concat_spellings(TokenList *tokens)
{
    StringBuilder sb;
    sb_init(&sb);
    for (size_t i = 0; i < tokens->n; ++i)
        sb_addstr(&sb, token_spelling(tokens->arr[i]));
    return sb_str(&sb);
}

Token *stringize_operator(TokenList *tokens)
{
    StringBuilder sb;

    sb_init(&sb);
    sb_add(&sb, '\"');

    for (size_t i = 0; i < tokens->n; ++i) {
        Token *token = tokens->arr[i];

        // Ignore whitespace before the first token, otherwise add single space
        // if lwhite was set by the lexer (per ISO/IEC 9899:1999 6.10.3.2)
        if (i > 0 && token->flags.lwhite)
            sb_add(&sb, ' ');

        switch (token->type) {
        case TK_IDENTIFIER:
        case TK_PP_NUMBER:
        case TK_OTHER:
            sb_addstr(&sb, token->data);
            break;
        case TK_CHAR_CONST:
        case TK_STRING_LIT:
            for (const char *s = token->data; *s; ++s)
                switch (*s) {
                case '\\':
                case '\"':
                    sb_add(&sb, '\\');
                    // FALLTHROUGH
                default:
                    sb_add(&sb, *s);
                    break;
                }
            break;
        default:
            sb_addstr(&sb, token_str[token->type]);
            break;
        }
    }

    sb_add(&sb, '\"');
    return create_token(TK_STRING_LIT, TOKEN_NOFLAGS, sb_str(&sb));
}

Token *glue_operator(Token *left, Token *right)
{
    // Combine the spelling of the two tokens (without whitespaces)
    StringBuilder sb;
    sb_init(&sb);
    sb_addstr(&sb, token_spelling(left));
    sb_addstr(&sb, token_spelling(right));
    char *combined = sb_str(&sb);

    // Fee old tokens
    free_token(left);
    free_token(right);

    // Re-lex new combined token
    LexCtx *ctx = lex_open_string("glue_tmp", combined);
    Token *result = lex_next(ctx);
    // If there are more tokens, it means glue failed
    if (lex_next(ctx))
        mcc_err("Token concatenation must result in one token");
    // Free buffers
    lex_free(ctx);
    free(combined);

    return result;
}
