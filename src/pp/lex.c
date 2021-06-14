// SPDX-License-Identifier: GPL-2.0-only

/*
 * Lexical analyzer
 */

#include <assert.h>
#include <stdio.h>
#include <lib/vec.h>
#include "token.h"
#include "lex.h"
#include "err.h"

typedef enum {
    LEX_FILE,
    LEX_STR,
} LexType;

struct LexCtx {
    // Name of the current file
    const char *filename;
    // Line number in the current file
    size_t line;
    // Is this the first token read from the context?
    _Bool first;

    // Type of data being lexed
    LexType type;
    union {
        FILE *fp;
        const char *str;
    };
};

static const char *get_filename(const char *filepath)
{
    const char *prev = filepath, *next;
    while ((next = strchr(prev, '/')))
        prev = next + 1;
    return prev;
}

LexCtx *lex_open_file(const char *filepath)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
        return NULL;

    LexCtx *ctx = calloc(1, sizeof *ctx);
    ctx->filename = get_filename(filepath);
    ctx->line = 1;
    ctx->first = 1;

    ctx->type = LEX_FILE;
    ctx->fp = fp;
    return ctx;
}

LexCtx *lex_open_string(const char *filename, const char *str)
{
    LexCtx *ctx = calloc(1, sizeof *ctx);
    ctx->filename = filename;
    ctx->line = 1;
    ctx->first = 1;

    ctx->type = LEX_STR;
    ctx->str = str;
    return ctx;
}

const char *lex_filename(LexCtx *ctx)
{
    // This API is only allowed to be called when a filename was specified
    assert(ctx->filename != NULL);

    return ctx->filename;
}

size_t lex_line(LexCtx *ctx)
{
    return ctx->line;
}

void lex_free(LexCtx *ctx)
{
    if (ctx->type == LEX_FILE)
        fclose(ctx->fp);
    free(ctx);
}

static int lex_getc(LexCtx *ctx)
{
    int ch = 0; // NOTE: initialization not needed
                // but GCC is not smart enough to tell

    switch (ctx->type) {
    case LEX_STR:
        ch = *ctx->str;
        if (!ch) {
            // Translate NUL-to EOF
            ch = EOF;
        } else {
            // Advance string to next character
            ++ctx->str;
        }
        break;
    case LEX_FILE:
        // Read character from stream
        ch = fgetc(ctx->fp);
        break;
    }

    return ch;
}

static void lex_ungetc(int ch, LexCtx *ctx)
{
    // Ungetting EOF is pointless
    if (ch == EOF)
        return;

    switch (ctx->type) {
    case LEX_STR:
        // Make sure this is used correctly
        assert(*--ctx->str == ch);
        break;
    case LEX_FILE:
        // Push character back to stream
        ungetc(ch, ctx->fp);
        break;
    }
}

static int lex_peek(LexCtx *ctx)
{
    int ch;

    ch = lex_getc(ctx);
    lex_ungetc(ch, ctx);
    return ch;
}

static _Bool lex_nextc(LexCtx *ctx, int want)
{
    int ch;

    ch = lex_getc(ctx);
    if (ch != want) {
        lex_ungetc(ch, ctx);
        return 0;
    }
    return 1;
}

static _Bool lex_nextstr(LexCtx *ctx, const char *want)
{
    int ch;
    const char *cur;

    for (cur = want; *cur; ++cur) {
        ch = lex_getc(ctx);
        if (ch != *cur) {
            lex_ungetc(ch, ctx);
            while (cur > want)
                lex_ungetc(*--cur, ctx);
            return 0;
        }
    }
    return 1;
}

static void identifier(int ch, LexCtx *ctx, Token *token)
{
    Vec_char buf;

    vec_char_init(&buf);
    vec_char_add(&buf, ch);

    for (;;)
        switch (lex_peek(ctx)) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            vec_char_add(&buf, lex_getc(ctx));
            break;
        default:
            token->type = TK_IDENTIFIER;
            token->data = vec_char_str(&buf);
            return;
        }
}

static void pp_num(int ch, LexCtx *ctx, Token *token)
{
    Vec_char buf;

    vec_char_init(&buf);
    vec_char_add(&buf, ch);

    for (;;)
        switch (lex_peek(ctx)) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ch = lex_getc(ctx);
            vec_char_add(&buf, ch);
            switch (ch) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (lex_peek(ctx)) {
                case '-':
                case '+':
                    vec_char_add(&buf, lex_getc(ctx));
                }
            }
            break;
        default:
            token->type = TK_PP_NUMBER;
            token->data = vec_char_str(&buf);
            return;
        }
}

#define GEN_LITERAL(name, tk_type, end)                                        \
static void name##_literal(int ch, LexCtx *ctx, Token *token)                  \
{                                                                              \
    Vec_char buf;                                                              \
    vec_char_init(&buf);                                                       \
    vec_char_add(&buf, ch);                                                    \
    for (;;) {                                                                 \
        ch = lex_getc(ctx);                                                    \
        switch (ch) {                                                          \
        default:                                                               \
            vec_char_add(&buf, ch);                                            \
            if (ch == '\\')                                                    \
                switch (lex_peek(ctx)) {                                       \
                case '\'':                                                     \
                case '\"':                                                     \
                    vec_char_add(&buf, lex_getc(ctx));                         \
                    break;                                                     \
                }                                                              \
            break;                                                             \
        case end:                                                              \
            vec_char_add(&buf, ch);                                            \
            token->type = tk_type;                                             \
            token->data = vec_char_str(&buf);                                  \
            return;                                                            \
        case EOF:                                                              \
        case '\n':                                                             \
            mcc_err("Unterminated " # name " literal");                        \
        }                                                                      \
    }                                                                          \
}

GEN_LITERAL(char,    TK_CHAR_CONST,  '\'')
GEN_LITERAL(wchar,   TK_WCHAR_CONST, '\'')
GEN_LITERAL(string,  TK_STRING_LIT,  '\"')
GEN_LITERAL(wstring, TK_WSTRING_LIT, '\"')
GEN_LITERAL(hchar,   TK_HCHAR_LIT,   '>')
GEN_LITERAL(qchar,   TK_QCHAR_LIT,   '\"')

Token *lex_next(LexCtx *ctx, _Bool want_header_name)
{
    Token *token;
    int ch;

    token = create_token(0, NULL);

    // First token from a lexer context can be a directive
    if (ctx->first) {
        token->directive = 1;
        ctx->first = 0;
    }

    retry:
    switch ((ch = lex_getc(ctx))) {
    // End of file
    case EOF:
        free_token(token);
        return NULL;
    // Invisible whitespaces (carriage return, form feed, vertical tab)
    case '\r':
    case '\f':
    case '\v':
        goto retry;
    // Visible whitespaces (tabs, spaces)
    case '\t':
    case ' ':
        token->lwhite = 1;
        goto retry;
    // End of line
    case '\n':
    newline:
        token->lwhite = 0;
        token->lnew = 1;
        token->directive = 1;
        goto retry;
    // Line concatenation
    case '\\':
        if (lex_nextc(ctx, '\n'))
            goto retry;
        break;
    // Identifier
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        if (ch == 'L')
            switch (lex_peek(ctx)) {
            case '\'':
                wchar_literal(lex_getc(ctx), ctx, token);
                return token;
            case '\"':
                wstring_literal(lex_getc(ctx), ctx, token);
                return token;
            }
        identifier(ch, ctx, token);
        return token;
    // PP-number
    case '0' ... '9':
        pp_num(ch, ctx, token);
        return token;
    // Character constant
    case '\'':
        char_literal(ch, ctx, token);
        return token;
    // String literal
    case '\"':
        if (want_header_name)
            qchar_literal(ch, ctx, token);
        else
            string_literal(ch, ctx, token);
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
        switch (lex_peek(ctx)) {
        case '0' ... '9':                              // PP-num
            pp_num(ch, ctx, token);
            return token;
        }
        if (lex_nextstr(ctx, ".."))                    // ...
            token->type = TK_VARARGS;
        else                                           // .
            token->type = TK_MEMBER;
        return token;
    case '-':
        if (lex_nextc(ctx, '>'))                       // ->
            token->type = TK_DEREF_MEMBER;
        else if (lex_nextc(ctx, '-'))                  // --
            token->type = TK_MINUS_MINUS;
        else if (lex_nextc(ctx, '='))                  // -=
            token->type = TK_SUB_EQUAL;
        else                                           // -
            token->type = TK_MINUS;
        return token;
    case '+':
        if (lex_nextc(ctx, '+'))                       // ++
            token->type = TK_PLUS_PLUS;
        else if (lex_nextc(ctx, '='))                  // +=
            token->type = TK_ADD_EQUAL;
        else                                           // +
            token->type = TK_PLUS;
        return token;
    case '&':
        if (lex_nextc(ctx, '&'))                       // &&
            token->type = TK_LOGIC_AND;
        else if (lex_nextc(ctx, '='))                  // &=
            token->type = TK_AND_EQUAL;
        else                                           // &
            token->type = TK_AMPERSAND;
        return token;
    case '*':
        if (lex_nextc(ctx, '='))                       // *=
            token->type = TK_MUL_EQUAL;
        else                                           // *
            token->type = TK_STAR;
        return token;
    case '!':
        if (lex_nextc(ctx, '='))                       // !=
            token->type = TK_NOT_EQUAL;
        else                                           // !
            token->type = TK_EXCL_MARK;
        return token;
    case '/':
        if (lex_nextc(ctx, '/')) {                     // Line comment
            while (lex_getc(ctx) != '\n');
            goto newline;
        }

        if (lex_nextc(ctx, '*')) {                     // Block comment
            for (;;) {
                ch = lex_getc(ctx);
                if (ch == EOF)
                    mcc_err("Unterminated block comment");
                if (ch == '*' && lex_nextc(ctx, '/')) {
                    token->lwhite = 1;
                    goto retry;
                }
            }
        }

        if (lex_nextc(ctx, '='))                       // /=
            token->type = TK_DIV_EQUAL;
        else                                           // /
            token->type = TK_FWD_SLASH;
        return token;
    case '%':
        if (lex_nextc(ctx, '=')) {                     // %=
            token->type = TK_REM_EQUAL;
        } else if (lex_nextc(ctx, '>')) {              // %>
            token->type = TK_RIGHT_CURLY;
        } else if (lex_nextc(ctx, ':')) {
            if (lex_nextstr(ctx, "%:"))                // %:%:
                token->type = TK_HASH_HASH;
            else                                       // %:
                token->type = TK_HASH;
        } else {                                       // %
            token->type = TK_PERCENT;
        }
        return token;
    case '<':
        if (want_header_name) {                        // System header name
            hchar_literal(ch, ctx, token);
            return token;
        }

        if (lex_nextc(ctx, '<')) {
            if (lex_nextc(ctx, '='))                   // <<=
                token->type = TK_LSHIFT_EQUAL;
            else                                       // <<
                token->type = TK_LEFT_SHIFT;
        } else if (lex_nextc(ctx, '=')) {              // <=
            token->type = TK_LESS_EQUAL;
        } else if (lex_nextc(ctx, ':')) {              // <:
            token->type = TK_LEFT_SQUARE;
        } else if (lex_nextc(ctx, '%')) {              // <%
            token->type = TK_LEFT_CURLY;
        } else {                                       // <
            token->type = TK_LEFT_ANGLE;
        }
        return token;
    case '>':
        if (lex_nextc(ctx, '>')) {
            if (lex_nextc(ctx, '='))                   // >>=
                token->type = TK_RSHIFT_EQUAL;
            else                                       // >>
                token->type = TK_RIGHT_SHIFT;
        } else if (lex_nextc(ctx, '=')) {              // >=
            token->type = TK_MORE_EQUAL;
        } else {                                       // >
            token->type = TK_RIGHT_ANGLE;
        }
        return token;
    case '=':
        if (lex_nextc(ctx, '='))                       // ==
            token->type = TK_EQUAL_EQUAL;
        else                                           // =
            token->type = TK_EQUAL;
        return token;
    case '^':
        if (lex_nextc(ctx, '='))                       // ^=
            token->type = TK_XOR_EQUAL;
        else                                           // ^
            token->type = TK_CARET;
        return token;
    case '|':
        if (lex_nextc(ctx, '|'))                       // ||
            token->type = TK_LOGIC_OR;
        else if (lex_nextc(ctx, '='))                  // |=
            token->type = TK_OR_EQUAL;
        else                                           // |
            token->type = TK_VERTICAL_BAR;
        return token;
    case ':':
        if (lex_nextc(ctx, '>'))                       // :>
            token->type = TK_RIGHT_SQUARE;
        else                                           // :
            token->type = TK_COLON;
        return token;
    case '#':
        if (lex_nextc(ctx, '#'))                       // ##
            token->type = TK_HASH_HASH;
        else                                           // #
            token->type = TK_HASH;
        return token;
    }

    // Couldn't lex token
    mcc_err("Unlexable character");
}
