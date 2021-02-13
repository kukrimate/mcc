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

static void lex_err(void)
{
    fprintf(stderr, "Invalid token!\n");
    exit(1);
}

static void identifier(int ch, Io *io, token *token)
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

static void pp_num(int ch, Io *io, token *token)
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

static void character(int ch, Io *io, token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = io_getc(io);
        switch (ch) {
        /* Normal character */
        default:
            VECc_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(ch, io, &buf);
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

static void string(int ch, Io *io, token *token)
{
    VECc buf;

    VECc_init(&buf);

    for (;;) {
        ch = io_getc(io);
        switch (ch) {
        /* Normal character */
        default:
            VECc_add(&buf, ch);
            break;
        /* Escape sequence */
        case '\\':
            escseq(ch, io, &buf);
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

static void lex_read(Io *io, token *token)
{
    int ch;

    token->lwhite = 0;
    // TODO: move this away from the lexer
    token->no_expand = 0;

    retry:
    switch ((ch = io_getc(io))) {
    // Whitespace
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
    case '\r':
        io_next(io, '\n');
        // FALLTHROUGH
    case '\n':
        token->type = TK_END_LINE;
        return;
    // Line concatanation
    case '\\':
        if (io_next(io, '\r')) {
            io_next(io, '\n');
            goto retry;
        } else if (io_next(io, '\n')) {
            goto retry;
        }
        lex_err();
        break;
    // Identifier
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        identifier(ch, io, token);
        return;
    // PP-number
    case '0' ... '9':
        pp_num(ch, io, token);
        return;
    // Character constant
    case '\'':
        character(ch, io, token);
        return;
    // String literal
    case '\"':
        string(ch, io, token);
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
        switch (io_peek(io)) {
        case '0' ... '9':                              // PP-num
            pp_num(ch, io, token);
            return;
        }
        if (io_nextstr(io, ".."))                      // ...
            token->type = TK_VARARGS;
        else                                           // .
            token->type = TK_MEMBER;
        return;
    case '-':
        if (io_next(io, '>'))                          // ->
            token->type = TK_DEREF_MEMBER;
        else if (io_next(io, '-'))                     // --
            token->type = TK_MINUS_MINUS;
        else if (io_next(io, '='))                     // -=
            token->type = TK_SUB_EQUAL;
        else                                           // -
            token->type = TK_MINUS;
        return;
    case '+':
        if (io_next(io, '+'))                          // ++
            token->type = TK_PLUS_PLUS;
        else if (io_next(io, '='))                     // +=
            token->type = TK_ADD_EQUAL;
        else                                           // +
            token->type = TK_PLUS;
        return;
    case '&':
        if (io_next(io, '&'))                          // &&
            token->type = TK_LOGIC_AND;
        else if (io_next(io, '='))                     // &=
            token->type = TK_AND_EQUAL;
        else                                           // &
            token->type = TK_AMPERSAND;
        return;
    case '*':
        if (io_next(io, '='))                          // *=
            token->type = TK_MUL_EQUAL;
        else                                           // *
            token->type = TK_STAR;
        return;
    case '!':
        if (io_next(io, '='))                          // !=
            token->type = TK_NOT_EQUAL;
        else                                           // !
            token->type = TK_EXCL_MARK;
        return;
    case '/':
        if (io_next(io, '/')) {                        // Line comment
            while (io_getc(io) != '\n');
            ++token->lwhite;
            goto retry;
        } else if (io_next(io, '*')) {                 // Block comment
            for (;;) {
                ch = io_getc(io);
                if (ch == EOF)
                    lex_err();
                if (ch == '*' && io_next(io, '/')) {
                    ++token->lwhite;
                    goto retry;
                }
            }
        }

        if (io_next(io, '='))                          // /=
            token->type = TK_DIV_EQUAL;
        else                                           // /
            token->type = TK_FWD_SLASH;
        return;
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
        return;
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
        return;
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
        return;
    case '=':
        if (io_next(io, '='))                          // ==
            token->type = TK_EQUAL_EQUAL;
        else                                           // =
            token->type = TK_EQUAL;
        return;
    case '^':
        if (io_next(io, '='))                          // ^=
            token->type = TK_XOR_EQUAL;
        else                                           // ^
            token->type = TK_CARET;
        return;
    case '|':
        if (io_next(io, '|'))                          // ||
            token->type = TK_LOGIC_OR;
        else if (io_next(io, '='))                     // |=
            token->type = TK_OR_EQUAL;
        else                                           // |
            token->type = TK_VERTICAL_BAR;
        return;
    case ':':
        if (io_next(io, '>'))                          // :>
            token->type = TK_RIGHT_SQUARE;
        else                                           // :
            token->type = TK_COLON;
        return;
    case '#':
        if (io_next(io, '#'))                          // ##
            token->type = TK_HASH_HASH;
        else                                           // #
            token->type = TK_HASH;
        return;
    default:
        lex_err();
    }
}

void lex_init(LexCtx *ctx, Io *io)
{
    ctx->io = io;
    VECtoken_init(&ctx->buffer);
}

void lex_next(LexCtx *ctx, token *token)
{
    if (ctx->buffer.n)
        *token = VECtoken_pop(&ctx->buffer, 0);
    else
        lex_read(ctx->io, token);
}

void lex_peek(LexCtx *ctx, token *token)
{
    if (!ctx->buffer.n)
        lex_read(ctx->io, VECtoken_push(&ctx->buffer));
    *token = ctx->buffer.arr[0];
}
