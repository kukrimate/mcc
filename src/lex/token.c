// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor token utility functions
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
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

Token *dup_token(Token *token)
{
    return create_token(token->type, token->flags,
        token->data ? strdup(token->data) : NULL);
}

void free_token(Token *token)
{
    if (token->data)
        free(token->data);
    free(token);
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

const char *token_spelling(Token *token)
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

void token_list_freeall(TokenList *list)
{
    for (size_t i = 0; i < list->n; ++i)
        free_token(list->arr[i]);
    token_list_free(list);
}

char *concat_spellings(TokenList *tokens)
{
    StringBuilder sb;
    sb_init(&sb);
    for (size_t i = 0; i < tokens->n; ++i)
        sb_addstr(&sb, token_spelling(tokens->arr[i]));
    return sb_str(&sb);
}
