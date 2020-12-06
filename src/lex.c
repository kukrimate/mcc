/*
 * Lexical analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include <vec.h>
#include "io.h"
#include "lex.h"

VEC_GEN(char, c)

static void
lex_err(void)
{
    fprintf(stderr, "Invalid token!\n");
    exit(1);
}

static void
identifier(int ch, FILE *fp, token *token)
{
    cvec buf;

    cvec_init(&buf);
    cvec_add(&buf, ch);

    for (;;)
        switch (mpeek(fp)) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            cvec_add(&buf, mgetc(fp));
            break;
        default:
            cvec_add(&buf, 0);
            token->type = TK_IDENTIFIER;
            token->data = buf.arr;
            return;
        }
}

static void
pp_num(int ch, FILE *fp, token *token)
{
    cvec buf;

    cvec_init(&buf);
    cvec_add(&buf, ch);

    for (;;)
        switch (mpeek(fp)) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ch = mgetc(fp);
            cvec_add(&buf, ch);
            switch (ch) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (mpeek(fp)) {
                case '-':
                case '+':
                    cvec_add(&buf, mgetc(fp));
                }
            }
            break;
        default:
            cvec_add(&buf, 0);
            token->type = TK_PP_NUMBER;
            token->data = buf.arr;
            return;
        }
}

static void
octal(FILE *fp, cvec *v, int ch)
{
    ch -= '0';
    /* Octal constants only allow 3 digits max */
    switch (mpeek(fp)) {
    case '0' ... '7':
        ch <<= 3;
        ch |= mgetc(fp) - '0';
        break;
    default:
        goto endc;
    }
    switch (mpeek(fp)) {
    case '0' ... '7':
        ch <<= 3;
        ch |= mgetc(fp) - '0';
        break;
    }
endc:
    cvec_add(v, ch);
}

static int
hexdigit_to_int(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    /* FIXME: assuming a-z and A-Z being continous is non-standard */
    if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
    /* Not a valid hex digit */
    lex_err();
    return 0; /* Never reached, but makes gcc shut up */
}

static void
hexadecimal(FILE *fp, cvec *v)
{
    int ch;

    ch = 0;
    /* Hex constants can be any length */
    for (;;)
        switch (mpeek(fp)) {
        case '0' ... '9':
        case 'a' ... 'f':
        case 'A' ... 'F':
            ch <<= 4;
            ch |= hexdigit_to_int(mgetc(fp));
            break;
        default:
            goto endloop;
        }
endloop:
    cvec_add(v, ch);
}

static void
escseq(FILE *fp, cvec *v, int ch)
{
    switch (ch = mgetc(fp)) {
    case '\'':  /* Simple escape sequences */
    case '"':
    case '?':
    case '\\':
        cvec_add(v, ch);
        break;
    case 'a':
        cvec_add(v, '\a');
        break;
    case 'b':
        cvec_add(v, '\b');
        break;
    case 'f':
        cvec_add(v, '\f');
        break;
    case 'n':
        cvec_add(v, '\n');
        break;
    case 'r':
        cvec_add(v, '\r');
        break;
    case 't':
        cvec_add(v, '\t');
        break;
    case 'v':
        cvec_add(v, '\v');
        break;
    case '0' ... '7': /* Octal */
        octal(fp, v, ch);
        break;
    case 'x':   /* Hexadecimal */
        hexadecimal(fp, v);
        break;
    /* Invalid escape sequence */
    default:
        lex_err();
    }
}

static void
character(int ch, FILE *fp, token *token)
{
    cvec buf;

    cvec_init(&buf);

    for (;;) {
        ch = mgetc(fp);
        switch (ch) {
        /* Normal character */
        default:
            cvec_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(fp, &buf, ch);
            break;
        /* End of character constant */
        case '\'':
            cvec_add(&buf, 0);
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

static void
string(int ch, FILE *fp, token *token)
{
    cvec buf;

    cvec_init(&buf);

    for (;;) {
        ch = mgetc(fp);
        switch (ch) {
        /* Normal character */
        default:
            cvec_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(fp, &buf, ch);
            break;
        /* End of string */
        case '\"':
            cvec_add(&buf, 0);
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

static void
header_name(int endch, FILE *fp, token *token)
{
    int ch;
    cvec buf;

    cvec_init(&buf);

    for (;;) {
        ch = mgetc(fp);
        switch (ch) {
        /* Normal character */
        default:
            /* End of header name */
            if (ch == endch) {
                cvec_add(&buf, 0);
                token->type = TK_HEADER_NAME;
                token->data = buf.arr;
                return;
            }
            cvec_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(fp, &buf, ch);
            break;
        /* Invalid string */
        case EOF:
        case '\n':
            lex_err();
        }
    }
}

void
lex_next_token(FILE *fp, token *token)
{
    int ch;

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
        if (mnext(fp, '.') == '.' && mnext(fp, '.') == '.') // ...
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
            goto retry;
        } else if (mnext(fp, '*') == '*') {                 // Block comment
            for (;;) {
                ch = mgetc(fp);
                if (ch == EOF)
                    lex_err();
                if (ch == '*' && mnext(fp, '/') == '/')
                    goto retry;
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
            if (mnext(fp, '%') == '%'
                    && mnext(fp, ':') == ':')               // %:%:
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
