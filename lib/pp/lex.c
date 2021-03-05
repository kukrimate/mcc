/*
 * Lexical analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include "str.h"
#include "io.h"
#include "token.h"
#include "lex.h"
#include "err.h"

static void identifier(int ch, Io *io, Token *token)
{
    Str buf;

    Str_init(&buf);
    Str_add(&buf, ch);

    for (;;)
        switch (io_peek(io)) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            Str_add(&buf, io_getc(io));
            break;
        default:
            token->type = TK_IDENTIFIER;
            token->data = Str_str(&buf);
            return;
        }
}

static void pp_num(int ch, Io *io, Token *token)
{
    Str buf;

    Str_init(&buf);
    Str_add(&buf, ch);

    for (;;)
        switch (io_peek(io)) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ch = io_getc(io);
            Str_add(&buf, ch);
            switch (ch) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (io_peek(io)) {
                case '-':
                case '+':
                    Str_add(&buf, io_getc(io));
                }
            }
            break;
        default:
            token->type = TK_PP_NUMBER;
            token->data = Str_str(&buf);
            return;
        }
}

#if 0
static void octal(int ch, Io *io, Str *v)
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
    Str_add(v, ch);
}

static void hexadecimal(int ch, Io *io, Str *v)
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
    Str_add(v, ch);
}

static void escseq(int ch, Io *io, Str *v)
{
    switch (ch = io_getc(io)) {
    case '\'':
    case '"':
    case '?':
    case '\\':
        Str_add(v, ch);
        break;
    case 'a':
        Str_add(v, '\a');
        break;
    case 'b':
        Str_add(v, '\b');
        break;
    case 'f':
        Str_add(v, '\f');
        break;
    case 'n':
        Str_add(v, '\n');
        break;
    case 'r':
        Str_add(v, '\r');
        break;
    case 't':
        Str_add(v, '\t');
        break;
    case 'v':
        Str_add(v, '\v');
        break;
    case '0' ... '7':
        octal(ch, io, v);
        break;
    case 'x':
        hexadecimal(ch, io, v);
        break;
    default:
        // Invalid escape sequence
        mcc_err("Invalid escape sequence");
    }
}
#endif

#define GEN_LITERAL(func_name, token_type, endch)   \
static void func_name(int ch, Io *io, Token *token) \
{                                                   \
    Str buf;                                        \
    Str_init(&buf);                                 \
    for (;;) {                                      \
        ch = io_getc(io);                           \
        switch (ch) {                               \
        /* Normal character */                      \
        default:                                    \
            Str_add(&buf, ch);                      \
            break;                                  \
        /* End of literal */                        \
        case endch:                                 \
            token->type = token_type;               \
            token->data = Str_str(&buf);            \
            return;                                 \
        /* Unterminated literal */                  \
        case EOF:                                   \
        case '\n':                                  \
            mcc_err("Unterminated literal");        \
        }                                           \
    }                                               \
}

// Generate literal parsing functions
GEN_LITERAL(string, TK_STRING_LIT, '\"')
GEN_LITERAL(character, TK_CHAR_CONST, '\'')
GEN_LITERAL(header_name, TK_HEADER_NAME, '>')

Token *lex_next(Io *io, _Bool want_header_name)
{
    Token *token;
    int ch;

    token = create_token(0, NULL);

    retry:
    switch ((ch = io_getc(io))) {
    // Whitespace
    case '\r':
    case '\f':
    case '\v':
    case '\t':
    case ' ':
        token->lwhite = 1;
        goto retry;
    // End of file
    case EOF:
        free_token(token);
        return NULL;
    // End of line
    case '\n':
        token->lnew = 1;
        goto retry;
    // Line concatanation
    case '\\':
        if (io_next(io, '\n'))
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
            while (io_getc(io) != '\n');
            token->lnew = 1;
            goto retry;
        }

        if (io_next(io, '*')) {                        // Block comment
            for (;;) {
                ch = io_getc(io);
                if (ch == EOF)
                    mcc_err("Unterminated block comment");
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
        if (want_header_name) {                        // Header name
            header_name(ch, io, token);
            return token;
        }

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
    mcc_err("Unlexable character");
}
