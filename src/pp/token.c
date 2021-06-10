// SPDX-License-Identifier: GPL-2.0-only

/*
 * Pre-processor token utility functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lib/vec.h>
#include "token.h"
#include "io.h"
#include "lex.h"
#include "err.h"

Token *create_token(TokenType type, char *data)
{
    Token *token;

    token = calloc(1, sizeof *token);
    token->type = type;
    token->data = data;
    return token;
}

Token *dup_token(Token *other)
{
    Token *token;

    token = calloc(1, sizeof *token);
    token->lnew = other->lnew;
    token->lwhite = other->lwhite;
    token->directive = other->directive;
    token->no_expand = other->no_expand;
    token->type = other->type;
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

static void token_to_str(Token *token, Vec_char *buf, _Bool want_white)
{
    char *str;

    if (want_white) {
        if (token->lnew)
            vec_char_add(buf, '\n');
        if (token->lwhite)
            vec_char_add(buf, ' ');
    }

    switch (token->type) {
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
        vec_char_addall(buf, token->data, strlen(token->data));
        break;
    case TK_CHAR_CONST:
        vec_char_add(buf, '\'');
        vec_char_addall(buf, token->data, strlen(token->data));
        vec_char_add(buf, '\'');
        break;
    case TK_STRING_LIT:
        vec_char_add(buf, '\"');
        vec_char_addall(buf, token->data, strlen(token->data));
        vec_char_add(buf, '\"');
        break;
    default:
        str = punctuator_str[token->type];
        vec_char_addall(buf, str, strlen(str));
        break;
    }
}

void output_token(Token *token)
{
    Vec_char buf;

    // Init buffer
    vec_char_init(&buf);
    // Vec_charingize token
    token_to_str(token, &buf, 1);
    // Print token
    printf("%s", vec_char_str(&buf));
    // Free buffer
    vec_char_free(&buf);
}

Token *stringize(Token *tokens)
{
    Vec_char buf;

    // Initialize buffer
    vec_char_init(&buf);
    // Vec_charingize tokens one-by-one
    for (; tokens; tokens = tokens->next)
        token_to_str(tokens, &buf, 1);
    // Add NUL-terminator
    vec_char_add(&buf, 0);

    // Create string literal token
    return create_token(TK_STRING_LIT, vec_char_str(&buf));
}

Token *glue(Token *left, Token *right)
{
    Vec_char buf;

    // Placemarker handling
    if (left->type == TK_PLACEMARKER && right->type == TK_PLACEMARKER)
        return create_token(TK_PLACEMARKER, NULL);
    if (left->type == TK_PLACEMARKER)
        return dup_token(right);
    if (right->type == TK_PLACEMARKER)
        return dup_token(left);

    // Initialize buffer
    vec_char_init(&buf);
    // Vec_charingize two tokens to buffer
    token_to_str(left, &buf, 0);
    token_to_str(right, &buf, 0);

    // Lex new buffer
    Io *io = io_open_string(vec_char_str(&buf));
    Token *result = lex_next(io, 0);
    result->lnew = left->lnew;
    result->lwhite = left->lwhite;
    // If there are more tokens, it means glue failed
    if (lex_next(io, 0))
        mcc_err("Token concatenation must result in one token");
    // Free buffers
    io_close(io);
    vec_char_free(&buf);

    return result;
}

