/*
 * Pre-processor token utility functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "token.h"
#include "io.h"
#include "lex.h"

VEC_GEN(char, c)

// Punctuator to string table
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

static void token_to_str(token *token, VECc *buf)
{
    char *str;

    if (token->lnew)
        VECc_add(buf, '\n');
    else if (token->lwhite)
        VECc_add(buf, ' ');

    switch (token->type) {
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
        VECc_addall(buf, token->data, strlen(token->data));
        break;
    case TK_CHAR_CONST:
        VECc_add(buf, '\'');
        VECc_addall(buf, token->data, strlen(token->data));
        VECc_add(buf, '\'');
        break;
    case TK_STRING_LIT:
        VECc_add(buf, '\"');
        VECc_addall(buf, token->data, strlen(token->data));
        VECc_add(buf, '\"');
        break;
    default:
        str = punctuator_str[token->type];
        VECc_addall(buf, str, strlen(str));
        break;
    }
}

void output_token(token *token)
{
    VECc buf;

    // Init buffer
    VECc_init(&buf);
    // Stringize token
    token_to_str(token, &buf);
    VECc_add(&buf, 0);
    // Print token
    printf("%s", buf.arr);
    // Free buffer
    VECc_free(&buf);
}

token stringize(VECtoken *tokens)
{
    VECc   buf;
    size_t i;
    token  result;

    // Initialize buffer
    VECc_init(&buf);

    // Stringize tokens one-by-one
    for (i = 0; i < tokens->n; ++i)
        token_to_str(tokens->arr + i, &buf);

    // Add NUL-terminator
    VECc_add(&buf, 0);

    // Create string literal token
    result.lwhite = 0;
    result.lnew = 0;
    result.no_expand = 0;
    result.type = TK_STRING_LIT;
    result.data = buf.arr;
    return result;
}

void pp_err(void);

token glue(token *left, token *right)
{
    VECc  buf;
    Io    *io;
    token tmp;
    _Bool fail;

    // Placemarker handling
    if (left->type == TK_PLACEMARKER) {
        // Two placemarkers just become one
        if (right->type == TK_PLACEMARKER)
            return *left;
        // If left is a placemarker return right
        return *right;
    }
    // If right is a placemarker return left
    if (right->type == TK_PLACEMARKER)
        return *left;

    // Initialize buffer
    VECc_init(&buf);
    // Stringize two tokens to buffer
    token_to_str(left, &buf);
    token_to_str(right, &buf);
    // Add NUL-terminator
    VECc_add(&buf, 0);

    // Lex new buffer
    fail = 0;
    io = io_open_string(buf.arr);
    tmp = lex_next(io);
    if (lex_next(io).type != TK_END_FILE)
        fail = 1;
    io_close(io);
    VECc_free(&buf);

    // Check if we failed
    if (fail)
        pp_err();

    return tmp;
}
