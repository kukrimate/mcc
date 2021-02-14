/*
 * Lexical analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "io.h"
#include "token.h"
#include "lex.h"

VEC_GEN(char, c)

void lex_err(void)
{
    fprintf(stderr, "Invalid token!\n");
    exit(1);
}

static void identifier(int ch, Io *io, Token *token)
{
    VECc buf;

    VECc_init(&buf);
    VECc_add(&buf, ch);

    for (;;)
        switch (io_peek(io)) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            VECc_add(&buf, io_getc(io));
            break;
        default:
            VECc_add(&buf, 0);
            token->type = TK_IDENTIFIER;
            token->data = buf.arr;
            return;
        }
}

static void pp_num(int ch, Io *io, Token *token)
{
    VECc buf;

    VECc_init(&buf);
    VECc_add(&buf, ch);

    for (;;)
        switch (io_peek(io)) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ch = io_getc(io);
            VECc_add(&buf, ch);
            switch (ch) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (io_peek(io)) {
                case '-':
                case '+':
                    VECc_add(&buf, io_getc(io));
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

static void octal(int ch, Io *io, VECc *v)
{
    ch -= '0';
    // Octal constants allow 3 digits max
    switch (io_peek(io)) {
    case '0' ... '7':
        ch = ch << 3 | (io_getc(io) - '0');
        break;
    default:
        goto endc;
    }
    switch (io_peek(io)) {
    case '0' ... '7':
        ch = ch << 3 | (io_getc(io) - '0');
        break;
    }
endc:
    VECc_add(v, ch);
}

static void hexadecimal(int ch, Io *io, VECc *v)
{
    ch = 0;
    // Hex constants can be any length
    for (;;)
        switch (io_peek(io)) {
        case '0' ... '9':
            ch = ch << 4 | (io_getc(io) - '0');
            break;
        case 'a' ... 'f':
            ch = ch << 4 | (io_getc(io) - 'a' + 0xa);
            break;
        case 'A' ... 'F':
            ch = ch << 4 | (io_getc(io) - 'A' + 0xa);
            break;
        default:
            goto endloop;
        }
endloop:
    VECc_add(v, ch);
}

static void escseq(int ch, Io *io, VECc *v)
{
    switch (ch = io_getc(io)) {
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
        octal(ch, io, v);
        break;
    case 'x':
        hexadecimal(ch, io, v);
        break;
    default:
        // Invalid escape sequence
        lex_err();
    }
}

static void character(int ch, Io *io, Token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = io_getc(io);
        switch (ch) {
        // Normal character
        default:
            VECc_add(&buf, ch);
            break;
        // Escape sequence
        case '\\':
            escseq(ch, io, &buf);
            break;
        // End of character constant
        case '\'':
            VECc_add(&buf, 0);
            token->type = TK_CHAR_CONST;
            token->data = buf.arr;
            return;
        // Invalid character constant
        case EOF:
        case '\n':
            lex_err();
        }
    }
}

static void string(int ch, Io *io, Token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = io_getc(io);
        switch (ch) {
        // Normal character
        default:
            VECc_add(&buf, ch);
            break;
        // Escape sequence
        case '\\':
            escseq(ch, io, &buf);
            break;
        // End of string
        case '\"':
            VECc_add(&buf, 0);
            token->type = TK_STRING_LIT;
            token->data = buf.arr;
            return;
        // Invalid string
        case EOF:
        case '\n':
            lex_err();
        }
    }
}

static _Bool next_nl(Io *io)
{
    if (io_next(io, '\r')) {
        io_next(io, '\n');
        return 1;
    }

    if (io_next(io, '\n'))
        return 1;

    return 0;
}

Token *lex_next(Io *io)
{
    Token *token;
    int ch;

    token = create_token(0, NULL);

    retry:
    switch ((ch = io_getc(io))) {
    // Whitespace
    case '\f':
    case '\v':
    case '\t':
    case ' ':
        token->lwhite = 1;
        goto retry;
    // End of file
    case EOF:
        free(token);
        return NULL;
    // End of line
    case '\r':
        io_next(io, '\n');
        // FALLTHROUGH
    case '\n':
        token->lnew = 1;
        goto retry;
    // Line concatanation
    case '\\':
        if (next_nl(io))
            goto retry;
        break;
    // Identifier
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        identifier(ch, io, token);
        return token;
    // PP-number
    case '0' ... '9':
        pp_num(ch, io, token);
        return token;
    // Character constant
    case '\'':
        character(ch, io, token);
        return token;
    // String literal
    case '\"':
        string(ch, io, token);
        return token;
    // Punctuators
    case '[':
        token->type = TK_LEFT_SQUARE;
        return token;
    case ']':
        token->type = TK_RIGHT_SQUARE;
        return token;
    case '(':
        token->type = TK_LEFT_PAREN;
        return token;
    case ')':
        token->type = TK_RIGHT_PAREN;
        return token;
    case '{':
        token->type = TK_LEFT_CURLY;
        return token;
    case '}':
        token->type = TK_RIGHT_CURLY;
        return token;
    case '~':
        token->type = TK_TILDE;
        return token;
    case '?':
        token->type = TK_QUEST_MARK;
        return token;
    case ';':
        token->type = TK_SEMICOLON;
        return token;
    case ',':
        token->type = TK_COMMA;
        return token;
    case '.':
        switch (io_peek(io)) {
        case '0' ... '9':                              // PP-num
            pp_num(ch, io, token);
            return token;
        }
        if (io_nextstr(io, ".."))                      // ...
            token->type = TK_VARARGS;
        else                                           // .
            token->type = TK_MEMBER;
        return token;
    case '-':
        if (io_next(io, '>'))                          // ->
            token->type = TK_DEREF_MEMBER;
        else if (io_next(io, '-'))                     // --
            token->type = TK_MINUS_MINUS;
        else if (io_next(io, '='))                     // -=
            token->type = TK_SUB_EQUAL;
        else                                           // -
            token->type = TK_MINUS;
        return token;
    case '+':
        if (io_next(io, '+'))                          // ++
            token->type = TK_PLUS_PLUS;
        else if (io_next(io, '='))                     // +=
            token->type = TK_ADD_EQUAL;
        else                                           // +
            token->type = TK_PLUS;
        return token;
    case '&':
        if (io_next(io, '&'))                          // &&
            token->type = TK_LOGIC_AND;
        else if (io_next(io, '='))                     // &=
            token->type = TK_AND_EQUAL;
        else                                           // &
            token->type = TK_AMPERSAND;
        return token;
    case '*':
        if (io_next(io, '='))                          // *=
            token->type = TK_MUL_EQUAL;
        else                                           // *
            token->type = TK_STAR;
        return token;
    case '!':
        if (io_next(io, '='))                          // !=
            token->type = TK_NOT_EQUAL;
        else                                           // !
            token->type = TK_EXCL_MARK;
        return token;
    case '/':
        if (io_next(io, '/')) {                        // Line comment
            while (!next_nl(io))
                io_getc(io);
            token->lwhite = 1;
            goto retry;
        } else if (io_next(io, '*')) {                 // Block comment
            for (;;) {
                ch = io_getc(io);
                if (ch == EOF)
                    lex_err();
                if (ch == '*' && io_next(io, '/')) {
                    token->lwhite = 1;
                    goto retry;
                }
            }
        }

        if (io_next(io, '='))                          // /=
            token->type = TK_DIV_EQUAL;
        else                                           // /
            token->type = TK_FWD_SLASH;
        return token;
    case '%':
        if (io_next(io, '=')) {                        // %=
            token->type = TK_REM_EQUAL;
        } else if (io_next(io, '>')) {                 // %>
            token->type = TK_RIGHT_CURLY;
        } else if (io_next(io, ':')) {
            if (io_nextstr(io, "%:"))                  // %:%:
                token->type = TK_HASH_HASH;
            else                                       // %:
                token->type = TK_HASH;
        } else {                                       // %
            token->type = TK_PERCENT;
        }
        return token;
    case '<':
        if (io_next(io, '<')) {
            if (io_next(io, '='))                      // <<=
                token->type = TK_LSHIFT_EQUAL;
            else                                       // <<
                token->type = TK_LEFT_SHIFT;
        } else if (io_next(io, '=')) {                 // <=
            token->type = TK_LESS_EQUAL;
        } else if (io_next(io, ':')) {                 // <:
            token->type = TK_LEFT_SQUARE;
        } else if (io_next(io, '%')) {                 // <%
            token->type = TK_LEFT_CURLY;
        } else {                                       // <
            token->type = TK_LEFT_ANGLE;
        }
        return token;
    case '>':
        if (io_next(io, '>')) {
            if (io_next(io, '='))                      // >>=
                token->type = TK_RSHIFT_EQUAL;
            else                                       // >>
                token->type = TK_RIGHT_SHIFT;
        } else if (io_next(io, '=')) {                 // >=
            token->type = TK_MORE_EQUAL;
        } else {                                       // >
            token->type = TK_RIGHT_ANGLE;
        }
        return token;
    case '=':
        if (io_next(io, '='))                          // ==
            token->type = TK_EQUAL_EQUAL;
        else                                           // =
            token->type = TK_EQUAL;
        return token;
    case '^':
        if (io_next(io, '='))                          // ^=
            token->type = TK_XOR_EQUAL;
        else                                           // ^
            token->type = TK_CARET;
        return token;
    case '|':
        if (io_next(io, '|'))                          // ||
            token->type = TK_LOGIC_OR;
        else if (io_next(io, '='))                     // |=
            token->type = TK_OR_EQUAL;
        else                                           // |
            token->type = TK_VERTICAL_BAR;
        return token;
    case ':':
        if (io_next(io, '>'))                          // :>
            token->type = TK_RIGHT_SQUARE;
        else                                           // :
            token->type = TK_COLON;
        return token;
    case '#':
        if (io_next(io, '#'))                          // ##
            token->type = TK_HASH_HASH;
        else                                           // #
            token->type = TK_HASH;
        return token;
    }

    // Couldn't lex token
    lex_err();
    abort();
}
