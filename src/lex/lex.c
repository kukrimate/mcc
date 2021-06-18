// SPDX-License-Identifier: GPL-2.0-only

//
// Lexical analyzer
//

#include <assert.h>
#include <stdio.h>
#include <vec.h>
#include "token.h"
#include "lex.h"

typedef enum {
    LEX_FILE,
    LEX_STR,
} LexType;

struct LexCtx {
    // Path to the current file
    char *path;
    // Line number in the current file
    size_t line;
    // Mark the next token as a directive
    _Bool directive;

    // Type of data being lexed
    LexType type;
    union {
        FILE *fp;
        const char *str;
    };

    // Buffered characters
    int ch1, ch2;
    // Ugly hack: Equivalent new line count for each character
    // For accurate line number tracking across line splices
    int ch1_lines, ch2_lines;
};

//
// Read the next character
//
static int lex_readc(LexCtx *ctx, int *lines)
{
    *lines = 0;
    if (ctx->type == LEX_STR) {
        if (ctx->str[0] == '\\' && ctx->str[1] == '\n') {
            ctx->str += 2;
            *lines += 1;
        }
        if (*ctx->str == '\n')
            *lines += 1;
        if (*ctx->str)
            return *ctx->str++;
        return EOF;
    } else {
        int ch1 = fgetc(ctx->fp);
        if (ch1 == '\\') {
            int ch2 = fgetc(ctx->fp);
            if (ch2 == '\n') {
                ch1 = fgetc(ctx->fp);
                *lines += 1;
            } else {
                ungetc(ch2, ctx->fp);
            }
        }
        if (ch1 == '\n')
            *lines += 1;
        return ch1;
    }
}

//
// Forward the stored characters
//
static void lex_fwd(LexCtx *ctx)
{
    // Skip over the newlines covered by ch1
    ctx->line += ctx->ch1_lines;
    // Overwrite ch1 with ch2
    ctx->ch1 = ctx->ch2;
    ctx->ch1_lines = ctx->ch2_lines;
    // Read next character into ch2
    ctx->ch2 = lex_readc(ctx, &ctx->ch2_lines);
}

//
// Match against one stored character
//
static _Bool lex_match1(LexCtx *ctx, int want)
{
    if (ctx->ch1 == want) {
        lex_fwd(ctx);
        return 1;
    }
    return 0;
}

//
// Match against two stored characters
//
static _Bool lex_match2(LexCtx *ctx, int want1, int want2)
{
    if (ctx->ch1 == want1 && ctx->ch2 == want2) {
        lex_fwd(ctx);
        lex_fwd(ctx);
        return 1;
    }
    return 0;
}

LexCtx *lex_open_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    LexCtx *ctx = calloc(1, sizeof *ctx);
    ctx->path = strdup(path);
    ctx->line = 1;
    ctx->directive = 1;

    ctx->type = LEX_FILE;
    ctx->fp = fp;

    ctx->ch1 = lex_readc(ctx, &ctx->ch1_lines);
    ctx->ch2 = lex_readc(ctx, &ctx->ch2_lines);
    return ctx;
}

LexCtx *lex_open_string(const char *path, const char *str)
{
    LexCtx *ctx = calloc(1, sizeof *ctx);
    ctx->path = strdup(path);
    ctx->line = 1;
    ctx->directive = 1;

    ctx->type = LEX_STR;
    ctx->str = str;

    ctx->ch1 = lex_readc(ctx, &ctx->ch1_lines);
    ctx->ch2 = lex_readc(ctx, &ctx->ch2_lines);
    return ctx;
}

const char *lex_path(LexCtx *ctx)
{
    return ctx->path;
}

size_t lex_line(LexCtx *ctx)
{
    return ctx->line;
}

void lex_free(LexCtx *ctx)
{
    if (ctx->type == LEX_FILE)
        fclose(ctx->fp);
    free(ctx->path);
    free(ctx);
}

static char *identifier(LexCtx *ctx)
{
    StringBuilder sb;
    sb_init(&sb);

    for (;; lex_fwd(ctx))
        switch (ctx->ch1) {
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            sb_add(&sb, ctx->ch1);
            break;
        default:
            return sb_str(&sb);
        }
}

static char *pp_num(LexCtx *ctx)
{
    StringBuilder sb;
    sb_init(&sb);

    for (;; lex_fwd(ctx))
        switch (ctx->ch1) {
        case '.':
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            sb_add(&sb, ctx->ch1);
            switch (ctx->ch1) {
            case 'e':
            case 'E':
            case 'p':
            case 'P':
                switch (ctx->ch2) {
                case '-':
                case '+':
                    sb_add(&sb, ctx->ch2);
                    lex_fwd(ctx);
                }
            }
            break;
        default:
            return sb_str(&sb);
        }
}

static char *char_const(LexCtx *ctx)
{
    StringBuilder sb;
    sb_init(&sb);

    if (lex_match1(ctx, 'L'))
        sb_add(&sb, 'L');

    sb_add(&sb, ctx->ch1);
    lex_fwd(ctx);

    for (;;) {
        if (ctx->ch1 == '\n' || ctx->ch1 == EOF) {
            fprintf(stderr, "Warning: Unterminated character constant\n");
            return sb_str(&sb);
        }

        if (lex_match2(ctx, '\\', '\'')) {
            sb_add(&sb, '\\');
            sb_add(&sb, '\'');
            continue;
        }

        if (lex_match1(ctx, '\'')) {
            sb_add(&sb, '\'');
            return sb_str(&sb);
        }

        sb_add(&sb, ctx->ch1);
        lex_fwd(ctx);
    }
}

static char *string_literal(LexCtx *ctx)
{
    StringBuilder sb;
    sb_init(&sb);

    if (lex_match1(ctx, 'L'))
        sb_add(&sb, 'L');

    sb_add(&sb, ctx->ch1);
    lex_fwd(ctx);

    for (;;) {
        if (ctx->ch1 == '\n' || ctx->ch1 == EOF) {
            fprintf(stderr, "Warning: Unterminated character constant\n");
            return sb_str(&sb);
        }

        if (lex_match2(ctx, '\\', '\"')) {
            sb_add(&sb, '\\');
            sb_add(&sb, '\"');
            continue;
        }

        if (lex_match1(ctx, '\"')) {
            sb_add(&sb, '\"');
            return sb_str(&sb);
        }

        sb_add(&sb, ctx->ch1);
        lex_fwd(ctx);
    }
}

static char *other(LexCtx *ctx)
{
    char ch = ctx->ch1;
    lex_fwd(ctx);
    return strndup(&ch, 1);
}

Token *lex_next(LexCtx *ctx)
{
    TokenFlags flags = TOKEN_NOFLAGS;

    if (ctx->directive) {
        ctx->directive = 0;
        flags.directive = 1;
    }

retry:
    switch (ctx->ch1) {
    case '_':
    case 'a' ... 'z':
    case 'A' ... 'Z':
        if (ctx->ch1 == 'L') {
            if (ctx->ch2 == '\'')
                return create_token(TK_CHAR_CONST, flags, char_const(ctx));
            if (ctx->ch2 == '\"')
                return create_token(TK_STRING_LIT, flags, string_literal(ctx));
        }
        return create_token(TK_IDENTIFIER, flags, identifier(ctx));
    case '.':
        switch (ctx->ch2) {
        case '0' ... '9':
            return create_token(TK_PP_NUMBER, flags, pp_num(ctx));
        }
        break;
    case '0' ... '9':
        return create_token(TK_PP_NUMBER, flags, pp_num(ctx));
    case '\'':
        return create_token(TK_CHAR_CONST, flags, char_const(ctx));
    case '\"':
        return create_token(TK_STRING_LIT, flags, string_literal(ctx));
    }

    if (lex_match1(ctx, EOF)) {
endfile:
        return NULL;
    }
    if (lex_match1(ctx, '\n')) {
newline:
        ctx->directive = 1;
        return create_token(TK_NEW_LINE, flags, NULL);
    }
    if (lex_match1(ctx, '\f') || lex_match1(ctx, '\r')
            || lex_match1(ctx, '\t') || lex_match1(ctx, '\v')
            || lex_match1(ctx, ' ')) {
whitespace:
        flags.lwhite = 1;
        goto retry;
    }
    if (lex_match1(ctx, '['))
        return create_token(TK_LEFT_SQUARE, flags, NULL);
    if (lex_match1(ctx, ']'))
        return create_token(TK_RIGHT_SQUARE, flags, NULL);
    if (lex_match1(ctx, '('))
        return create_token(TK_LEFT_PAREN, flags, NULL);
    if (lex_match1(ctx, ')'))
        return create_token(TK_RIGHT_PAREN, flags, NULL);
    if (lex_match1(ctx, '{'))
        return create_token(TK_LEFT_CURLY, flags, NULL);
    if (lex_match1(ctx, '}'))
        return create_token(TK_RIGHT_CURLY, flags, NULL);
    if (lex_match1(ctx, '~'))
        return create_token(TK_TILDE, flags, NULL);
    if (lex_match1(ctx, '?'))
        return create_token(TK_QUEST_MARK, flags, NULL);
    if (lex_match1(ctx, ';'))
        return create_token(TK_SEMICOLON, flags, NULL);
    if (lex_match1(ctx, ','))
        return create_token(TK_COMMA, flags, NULL);
    if (lex_match1(ctx, '.')) {
        if (lex_match2(ctx, '.', '.'))                 // ...
            return create_token(TK_VARARGS, flags, NULL);
        else                                           // .
            return create_token(TK_MEMBER, flags, NULL);
    }
    if (lex_match1(ctx, '-')) {
        if (lex_match1(ctx, '>'))                      // ->
            return create_token(TK_DEREF_MEMBER, flags, NULL);
        else if (lex_match1(ctx, '-'))                 // --
            return create_token(TK_MINUS_MINUS, flags, NULL);
        else if (lex_match1(ctx, '='))                 // -=
            return create_token(TK_SUB_EQUAL, flags, NULL);
        else                                           // -
            return create_token(TK_MINUS, flags, NULL);
    }
    if (lex_match1(ctx, '+')) {
        if (lex_match1(ctx, '+'))                      // ++
            return create_token(TK_PLUS_PLUS, flags, NULL);
        else if (lex_match1(ctx, '='))                 // +=
            return create_token(TK_ADD_EQUAL, flags, NULL);
        else                                           // +
            return create_token(TK_PLUS, flags, NULL);
    }

    if (lex_match1(ctx, '&')) {
        if (lex_match1(ctx, '&'))                      // &&
            return create_token(TK_LOGIC_AND, flags, NULL);
        else if (lex_match1(ctx, '='))                 // &=
            return create_token(TK_AND_EQUAL, flags, NULL);
        else                                           // &
            return create_token(TK_AMPERSAND, flags, NULL);
    }
    if (lex_match1(ctx, '*')) {
        if (lex_match1(ctx, '='))                      // *=
            return create_token(TK_MUL_EQUAL, flags, NULL);
        else                                           // *
            return create_token(TK_STAR, flags, NULL);
    }
    if (lex_match1(ctx, '!')) {
        if (lex_match1(ctx, '='))                      // !=
            return create_token(TK_NOT_EQUAL, flags, NULL);
        else                                           // !
            return create_token(TK_EXCL_MARK, flags, NULL);
    }
    if (lex_match1(ctx, '/')) {
        if (lex_match1(ctx, '/'))                      // Line comment
            for (;; lex_fwd(ctx)) {
                if (lex_match1(ctx, EOF))
                    goto endfile;
                if (lex_match1(ctx, '\n'))
                    goto newline;
            }
        if (lex_match1(ctx, '*'))                      // Block comment
            for (;; lex_fwd(ctx)) {
                if (lex_match1(ctx, EOF))
                    goto endfile;
                if (lex_match2(ctx, '*', '/'))
                    goto whitespace;
            }
        if (lex_match1(ctx, '='))                      // /=
            return create_token(TK_DIV_EQUAL, flags, NULL);
        else                                           // /
            return create_token(TK_FWD_SLASH, flags, NULL);
    }
    if (lex_match1(ctx, '%')) {
        if (lex_match1(ctx, '=')) {                    // %=
            return create_token(TK_REM_EQUAL, flags, NULL);
        } else if (lex_match1(ctx, '>')) {             // %>
            return create_token(TK_RIGHT_CURLY, flags, NULL);
        } else if (lex_match1(ctx, ':')) {
            if (lex_match2(ctx, '%', ':'))             // %:%:
                return create_token(TK_HASH_HASH, flags, NULL);
            else                                       // %:
                return create_token(TK_HASH, flags, NULL);
        } else {                                       // %
            return create_token(TK_PERCENT, flags, NULL);
        }
    }
    if (lex_match1(ctx, '<')) {
        if (lex_match1(ctx, '<')) {
            if (lex_match1(ctx, '='))                  // <<=
                return create_token(TK_LSHIFT_EQUAL, flags, NULL);
            else                                       // <<
                return create_token(TK_LEFT_SHIFT, flags, NULL);
        } else if (lex_match1(ctx, '=')) {             // <=
            return create_token(TK_LESS_EQUAL, flags, NULL);
        } else if (lex_match1(ctx, ':')) {             // <:
            return create_token(TK_LEFT_SQUARE, flags, NULL);
        } else if (lex_match1(ctx, '%')) {             // <%
            return create_token(TK_LEFT_CURLY, flags, NULL);
        } else {                                       // <
            return create_token(TK_LEFT_ANGLE, flags, NULL);
        }
    }
    if (lex_match1(ctx, '>')) {
        if (lex_match1(ctx, '>')) {
            if (lex_match1(ctx, '='))                  // >>=
                return create_token(TK_RSHIFT_EQUAL, flags, NULL);
            else                                       // >>
                return create_token(TK_RIGHT_SHIFT, flags, NULL);
        } else if (lex_match1(ctx, '=')) {             // >=
            return create_token(TK_MORE_EQUAL, flags, NULL);
        } else {                                       // >
            return create_token(TK_RIGHT_ANGLE, flags, NULL);
        }
    }
    if (lex_match1(ctx, '=')) {
        if (lex_match1(ctx, '='))                      // ==
            return create_token(TK_EQUAL_EQUAL, flags, NULL);
        else                                           // =
            return create_token(TK_EQUAL, flags, NULL);
    }
    if (lex_match1(ctx, '^')) {
        if (lex_match1(ctx, '='))                      // ^=
            return create_token(TK_XOR_EQUAL, flags, NULL);
        else                                           // ^
            return create_token(TK_CARET, flags, NULL);
    }
    if (lex_match1(ctx, '|')) {
        if (lex_match1(ctx, '|'))                      // ||
            return create_token(TK_LOGIC_OR, flags, NULL);
        else if (lex_match1(ctx, '='))                 // |=
            return create_token(TK_OR_EQUAL, flags, NULL);
        else                                           // |
            return create_token(TK_VERTICAL_BAR, flags, NULL);
    }
    if (lex_match1(ctx, ':')) {
        if (lex_match1(ctx, '>'))                      // :>
            return create_token(TK_RIGHT_SQUARE, flags, NULL);
        else                                           // :
            return create_token(TK_COLON, flags, NULL);
    }
    if (lex_match1(ctx, '#')) {
        if (lex_match1(ctx, '#'))                      // ##
            return create_token(TK_HASH_HASH, flags, NULL);
        else                                           // #
            return create_token(TK_HASH, flags, NULL);
    }

    return create_token(TK_OTHER, flags, other(ctx));
}
