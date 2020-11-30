/*
 * Lexical analyzer
 */

#include <stdio.h>
#include <vec.h>
#include "lex.h"

void
next_token(FILE *fp, token *token)
{
    int ch;

    switch ((ch = fgetc(fp))) {
    /* End of file */
    case EOF:
        token->type = TK_END_FILE;
        token->data = NULL;
        return;
    /* End of line */
    case '\n':
        token->type = TK_END_LINE;
        token->data = NULL;
        return;
    /* Identifier */
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        token->type = TK_IDENTIFIER;
        break;
    }
}
