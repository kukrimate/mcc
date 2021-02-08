/*
 * Lexical analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "io.h"
#include "lex.h"

VEC_GEN(char, c)

static void lex_err(void)
{
    fprintf(stderr, "Invalid token!\n");
    exit(1);
}

static void identifier(int ch, FILE *fp, token *token)
{
    VECc buf;

    VECc_init(&buf);
    VECc_add(&buf, ch);

    for (;;)
        switch (mpeek(fp)) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            VECc_add(&buf, mgetc(fp));
            break;
        default:
            VECc_add(&buf, 0);
            token->type = TK_IDENTIFIER;
            token->data = buf.arr;
            return;
        }
}

static void pp_num(int ch, FILE *fp, token *token)
{
    VECc buf;

    VECc_init(&buf);
    VECc_add(&buf, ch);

    for (;;)
        switch (mpeek(fp)) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ch = mgetc(fp);
            VECc_add(&buf, ch);
            switch (ch) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (mpeek(fp)) {
                case '-':
                case '+':
                    VECc_add(&buf, mgetc(fp));
                }
            }
            break;
        default:
            VECc_add(&buf, 0);
            token->type = TK_PP_NUMBER;
            token->data = buf.arr;
            return;
        }
}

static void octal(int ch, FILE *fp, VECc *v)
{
    ch -= '0';
    // Octal constants allow 3 digits max
    switch (mpeek(fp)) {
    case '0' ... '7':
        ch = ch << 3 | (mgetc(fp) - '0');
        break;
    default:
        goto endc;
    }
    switch (mpeek(fp)) {
    case '0' ... '7':
        ch = ch << 3 | (mgetc(fp) - '0');
        break;
    }
endc:
    VECc_add(v, ch);
}

static void hexadecimal(int ch, FILE *fp, VECc *v)
{
    ch = 0;
    // Hex constants can be any length
    for (;;)
        switch (mpeek(fp)) {
        case '0' ... '9':
            ch = ch << 4 | (mgetc(fp) - '0');
            break;
        case 'a' ... 'f':
            ch = ch << 4 | (mgetc(fp) - 'a' + 0xa);
            break;
        case 'A' ... 'F':
            ch = ch << 4 | (mgetc(fp) - 'A' + 0xa);
            break;
        default:
            goto endloop;
        }
endloop:
    VECc_add(v, ch);
}

static void escseq(int ch, FILE *fp, VECc *v)
{
    switch (ch = mgetc(fp)) {
    case '\'':
    case '"':
    case '?':
    case '\\':
        VECc_add(v, ch);
        break;
    case 'a':
        VECc_add(v, '\a');
        break;
    case 'b':
        VECc_add(v, '\b');
        break;
    case 'f':
        VECc_add(v, '\f');
        break;
    case 'n':
        VECc_add(v, '\n');
        break;
    case 'r':
        VECc_add(v, '\r');
        break;
    case 't':
        VECc_add(v, '\t');
        break;
    case 'v':
        VECc_add(v, '\v');
        break;
    case '0' ... '7':
        octal(ch, fp, v);
        break;
    case 'x':
        hexadecimal(ch, fp, v);
        break;
    default:
        // Invalid escape sequence
        lex_err();
    }
}

static void character(int ch, FILE *fp, token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = mgetc(fp);
        switch (ch) {
        /* Normal character */
        default:
            VECc_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(ch, fp, &buf);
            break;
        /* End of character constant */
        case '\'':
            VECc_add(&buf, 0);
            token->type = TK_CHAR_CONST;
            token->data = buf.arr;
            return;
        /* Invalid character constant */
        case EOF:
        case '\n':
            lex_err();
        }
    }
}

static void string(int ch, FILE *fp, token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = mgetc(fp);
        switch (ch) {
        /* Normal character */
        default:
            VECc_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(ch, fp, &buf);
            break;
        /* End of string */
        case '\"':
            VECc_add(&buf, 0);
            token->type = TK_STRING_LIT;
            token->data = buf.arr;
            return;
        /* Invalid string */
        case EOF:
        case '\n':
            lex_err();
        }
    }
}

void lex_next_token(FILE *fp, token *token)
{
    int ch;

    token->lwhite = 0;
    // TODO: move this away from the lexer
    token->no_expand = 0;

    retry:
    switch ((ch = mgetc(fp))) {
    // Whitespace
    case '\r':
    case '\f':
    case '\v':
    case '\t':
    case ' ':
        ++token->lwhite;
        goto retry;
    // End of file
    case EOF:
        token->type = TK_END_FILE;
        return;
    // End of line
    case '\n':
        token->type = TK_END_LINE;
        return;
    // Identifier
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        identifier(ch, fp, token);
        return;
    // PP-number
    case '0' ... '9':
        pp_num(ch, fp, token);
        return;
    // Character constant
    case '\'':
        character(ch, fp, token);
        return;
    // String literal
    case '\"':
        string(ch, fp, token);
        return;
    // Punctuators
    case '[':
        token->type = TK_LEFT_SQUARE;
        return;
    case ']':
        token->type = TK_RIGHT_SQUARE;
        return;
    case '(':
        token->type = TK_LEFT_PAREN;
        return;
    case ')':
        token->type = TK_RIGHT_PAREN;
        return;
    case '{':
        token->type = TK_LEFT_CURLY;
        return;
    case '}':
        token->type = TK_RIGHT_CURLY;
        return;
    case '~':
        token->type = TK_TILDE;
        return;
    case '?':
        token->type = TK_QUEST_MARK;
        return;
    case ';':
        token->type = TK_SEMICOLON;
        return;
    case ',':
        token->type = TK_COMMA;
        return;
    case '.':
        switch (mpeek(fp)) {
        case '0' ... '9':                                   // PP-num
            pp_num(ch, fp, token);
            return;
        }
        if (mnextstr(fp, ".."))                             // ...
            token->type = TK_VARARGS;
        else                                                // .
            token->type = TK_MEMBER;
        return;
    case '-':
        if (mnext(fp, '>') == '>')                          // ->
            token->type = TK_DEREF_MEMBER;
        else if (mnext(fp, '-') == '-')                     // --
            token->type = TK_MINUS_MINUS;
        else if (mnext(fp, '=') == '=')                     // -=
            token->type = TK_SUB_EQUAL;
        else                                                // -
            token->type = TK_MINUS;
        return;
    case '+':
        if (mnext(fp, '+') == '+')                          // ++
            token->type = TK_PLUS_PLUS;
        else if (mnext(fp, '=') == '=')                     // +=
            token->type = TK_ADD_EQUAL;
        else                                                // +
            token->type = TK_PLUS;
        return;
    case '&':
        if (mnext(fp, '&') == '&')                          // &&
            token->type = TK_LOGIC_AND;
        else if (mnext(fp, '=') == '=')                     // &=
            token->type = TK_AND_EQUAL;
        else                                                // &
            token->type = TK_AMPERSAND;
        return;
    case '*':
        if (mnext(fp, '=') == '=')                          // *=
            token->type = TK_MUL_EQUAL;
        else                                                // *
            token->type = TK_STAR;
        return;
    case '!':
        if (mnext(fp, '=') == '=')                          // !=
            token->type = TK_NOT_EQUAL;
        else                                                // !
            token->type = TK_EXCL_MARK;
        return;
    case '/':
        if (mnext(fp, '/') == '/') {                        // Line comment
            while (mgetc(fp) != '\n');
            ++token->lwhite;
            goto retry;
        } else if (mnext(fp, '*') == '*') {                 // Block comment
            for (;;) {
                ch = mgetc(fp);
                if (ch == EOF)
                    lex_err();
                if (ch == '*' && mnext(fp, '/') == '/') {
                    ++token->lwhite;
                    goto retry;
                }
            }
        }

        if (mnext(fp, '=') == '=')                          // /=
            token->type = TK_DIV_EQUAL;
        else                                                // /
            token->type = TK_FWD_SLASH;
        return;
    case '%':
        if (mnext(fp, '=') == '=') {                        // %=
            token->type = TK_REM_EQUAL;
        } else if (mnext(fp, '>') == '>') {                 // %>
            token->type = TK_RIGHT_CURLY;
        } else if (mnext(fp, ':') == ':') {
            if (mnextstr(fp, "%:"))                         // %:%:
                token->type = TK_HASH_HASH;
            else                                            // %:
                token->type = TK_HASH;
        } else {                                            // %
            token->type = TK_PERCENT;
        }
        return;
    case '<':
        if (mnext(fp, '<') == '<') {
            if (mnext(fp, '=') == '=')                      // <<=
                token->type = TK_LSHIFT_EQUAL;
            else                                            // <<
                token->type = TK_LEFT_SHIFT;
        } else if (mnext(fp, '=') == '=') {                 // <=
            token->type = TK_LESS_EQUAL;
        } else if (mnext(fp, ':') == ':') {                 // <:
            token->type = TK_LEFT_SQUARE;
        } else if (mnext(fp, '%') == '%') {                 // <%
            token->type = TK_LEFT_CURLY;
        } else {                                            // <
            token->type = TK_LEFT_ANGLE;
        }
        return;
    case '>':
        if (mnext(fp, '>') == '>') {
            if (mnext(fp, '=') == '=')                      // >>=
                token->type = TK_RSHIFT_EQUAL;
            else                                            // >>
                token->type = TK_RIGHT_SHIFT;
        } else if (mnext(fp, '=') == '=') {                 // >=
            token->type = TK_MORE_EQUAL;
        } else {                                            // >
            token->type = TK_RIGHT_ANGLE;
        }
        return;
    case '=':
        if (mnext(fp, '=') == '=')                          // ==
            token->type = TK_EQUAL_EQUAL;
        else                                                // =
            token->type = TK_EQUAL;
        return;
    case '^':
        if (mnext(fp, '=') == '=')                          // ^=
            token->type = TK_XOR_EQUAL;
        else                                                // ^
            token->type = TK_CARET;
        return;
    case '|':
        if (mnext(fp, '|') == '|')                          // ||
            token->type = TK_LOGIC_OR;
        else if (mnext(fp, '=') == '=')                     // |=
            token->type = TK_OR_EQUAL;
        else                                                // |
            token->type = TK_VERTICAL_BAR;
        return;
    case ':':
        if (mnext(fp, '>') == '>')                          // :>
            token->type = TK_RIGHT_SQUARE;
        else                                                // :
            token->type = TK_COLON;
        return;
    case '#':
        if (mnext(fp, '#') == '#')                          // ##
            token->type = TK_HASH_HASH;
        else                                                // #
            token->type = TK_HASH;
        return;
    default:
        lex_err();
    }
}

// Punctuator to string table
const char *punctuator_str[] = {
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
