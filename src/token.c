/*
 * Pre-processor token utility functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "token.h"

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

void output_token(token *token)
{
    if (token->lwhite)
        putchar(' ');
    else if (token->lnew)
        putchar('\n');

    switch (token->type) {
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
        printf("%s", token->data);
        break;
    case TK_CHAR_CONST:
        printf("\'%s\'", token->data);
        break;
    case TK_STRING_LIT:
        printf("\"%s\"", token->data);
        break;
    default:
        printf("%s", punctuator_str[token->type]);
    }
}

token stringize(VECtoken *tokens)
{
    VECc buf;
    size_t i;
    token *cur, result;

    // Initialize buffer
    VECc_init(&buf);

    // Stringize tokens one-by-one
    for (i = 0; i < tokens->n; ++i) {
        cur = tokens->arr + i;

        if (cur->lwhite)
            VECc_add(&buf, ' ');
        else if (cur->lnew)
            VECc_add(&buf, '\n');

        switch (cur->type) {
        case TK_IDENTIFIER:
        case TK_PP_NUMBER:
            VECc_addall(&buf, cur->data, strlen(cur->data));
            break;
        case TK_CHAR_CONST:
            VECc_add(&buf, '\'');
            VECc_addall(&buf, cur->data, strlen(cur->data));
            VECc_add(&buf, '\'');
            break;
        case TK_STRING_LIT:
            VECc_add(&buf, '\"');
            VECc_addall(&buf, cur->data, strlen(cur->data));
            VECc_add(&buf, '\"');
            break;
        default:
            VECc_addall(&buf, punctuator_str[cur->type],
                        strlen(punctuator_str[cur->type]));
            break;
        }
    }

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
