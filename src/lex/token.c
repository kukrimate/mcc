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
    token->type = type;
    token->flags = flags;
    token->data = data;
    return token;
}

Token *dup_token(Token *other)
{
    Token *token;

    token = calloc(1, sizeof *token);
    token->type = other->type;
    token->flags = other->flags;
    if (other->data)
        token->data = strdup(other->data);
    token->next = NULL;
    return token;
}

Token *dup_tokens(Token *head)
{
    Token *result, **tail;

    result = NULL;
    tail = &result;

    for (; head; head = head->next) {
        *tail = dup_token(head);
        tail = &(*tail)->next;
    }

    return result;
}

void free_token(Token *token)
{
    if (token->data)
        free(token->data);
    free(token);
}

void free_tokens(Token *head)
{
    Token *tmp;

    while (head) {
        tmp = head->next;
        free_token(head);
        head = tmp;
    }
}

// Punctuator to string conversion table
static char *punctuator_str[] = {
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
    [TK_PLACEMARKER  ] = "$", // This should never be printed
};

static const char *token_spelling(Token *token)
{
    switch (token->type) {
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
    case TK_CHAR_CONST:
    case TK_STRING_LIT:
        return token->data;
    default:
        return punctuator_str[token->type];
    }
}

void output_token(Token *token)
{
    if (token->flags.lnew)
        putchar('\n');
    if (token->flags.lwhite)
        putchar(' ');
    fputs(token_spelling(token), stdout);
}

Token *stringize(Token *tokens)
{
    Vec_char buf;

    vec_char_init(&buf);
    vec_char_add(&buf, '\"');

    _Bool first = 1;
    for (; tokens; tokens = tokens->next) {
        // Add whitespaces if this is not the first token
        if (first)
            first = 0;
        else if (tokens->flags.lnew || tokens->flags.lwhite)
            vec_char_add(&buf, ' ');

        switch (tokens->type) {
        case TK_IDENTIFIER:
        case TK_PP_NUMBER:
            vec_char_addall(&buf, tokens->data, strlen(tokens->data));
            break;
        case TK_CHAR_CONST:
        case TK_STRING_LIT:
            for (const char *s = tokens->data; *s; ++s)
                switch (*s) {
                case '\\':
                case '\"':
                    vec_char_add(&buf, '\\');
                    // FALLTHROUGH
                default:
                    vec_char_add(&buf, *s);
                    break;
                }
            break;
        default:
            vec_char_addall(&buf, punctuator_str[tokens->type],
                strlen(punctuator_str[tokens->type]));
            break;
        }
    }

    vec_char_add(&buf, '\"');
    return create_token(TK_STRING_LIT, TOKEN_NOFLAGS, vec_char_str(&buf));
}

//
// Concatenate two strings into a newly-allocated string
//
static char *strcat_alloc(const char *s1, const char *s2)
{
    size_t l1 = strlen(s1), l2 = strlen(s2);
    char *result = malloc(l1 + l2 + 1);
    memcpy(result, s1, l1);
    memcpy(result + l1, s2, l2);
    result[l1 + l2] = 0;
    return result;
}

Token *glue(Token *left, Token *right)
{
    // Placemarker handling
    if (left->type == TK_PLACEMARKER && right->type == TK_PLACEMARKER)
        return create_token(TK_PLACEMARKER, TOKEN_NOFLAGS, NULL);
    if (left->type == TK_PLACEMARKER)
        return dup_token(right);
    if (right->type == TK_PLACEMARKER)
        return dup_token(left);

    // Combine the spelling of the two tokens (without whitespaces)
    char *combined = strcat_alloc(token_spelling(left),
                                    token_spelling(right));
    // Lex new buffer
    LexCtx *ctx = lex_open_string("glue_tmp", combined);
    Token *result = lex_next(ctx);
    result->flags = left->flags;
    // If there are more tokens, it means glue failed
    if (lex_next(ctx))
        mcc_err("Token concatenation must result in one token");
    // Free buffers
    lex_free(ctx);
    free(combined);

    return result;
}

char *concat_spellings(Token *head)
{
    Vec_char buf;
    vec_char_init(&buf);
    for (; head; head = head->next) {
        const char *spelling = token_spelling(head);
        vec_char_addall(&buf, spelling, strlen(spelling));
    }
    return vec_char_str(&buf);
}
