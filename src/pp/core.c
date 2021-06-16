// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: core logic
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <vec.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

static void handle_date(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("Mar  7 2021")));
}

static void handle_time(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, TOKEN_NOFLAGS, strdup("15:30:07")));
}

static void handle_file(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, TOKEN_NOFLAGS, strdup("unknown")));
}

static void handle_line(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, TOKEN_NOFLAGS, strdup("1")));
}

static void handle_one(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
     create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("1")));
}

static void handle_vers(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
     create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("199901L")));
}

//
// Pre-defined macros
//
static Predef predefs[] = {
    // Required by ISO/IEC 9899:1999
    { "__DATE__",         &handle_date },
    { "__TIME__",         &handle_time },
    { "__FILE__",         &handle_file },
    { "__LINE__",         &handle_line },
    { "__STDC__",         &handle_one  },
    { "__STDC_HOSTED__",  &handle_one  },
    { "__STDC_VERSION__", &handle_vers },
    // These are needed to keep glibc happy
    { "__x86_64__",       &handle_one  },
    { "__amd64",          &handle_one  },
    { "__amd64__",        &handle_one  },
    { "__LP64__",         &handle_one  },
    { "_LP64",            &handle_one  },
    { "__ELF__",          &handle_one  },
    { "__gnu_linux__",    &handle_one  },
    { "__linux",          &handle_one  },
    { "__linux__",        &handle_one  },
    { "__unix",           &handle_one  },
    { "__unix__",         &handle_one  },
};

Predef *find_predef(Token *identifier)
{
    size_t i;

    if (!identifier || identifier->type != TK_IDENTIFIER)
        return NULL;
    for (i = 0; i < sizeof predefs / sizeof *predefs; ++i)
        if (!strcmp(predefs[i].name, identifier->data))
            return predefs + i;
    return NULL;
}

void __attribute__((noreturn)) pp_err(PpContext *ctx, const char *err, ...)
{
    while (ctx->parent)
        ctx = ctx->parent;
    Frame *file_frame = ctx->frames;
    while (file_frame->type != F_LEXER) {
        file_frame = file_frame->next;
        assert(file_frame != NULL);
    }

    fflush(stdout);
    fprintf(stderr, "Error: %s:%ld: ", lex_path(file_frame->lex),
        lex_line(file_frame->lex));
    va_list ap;
    va_start(ap, err);
    vfprintf(stderr, err, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static Frame *new_frame(PpContext *ctx)
{
    Frame *frame = calloc(1, sizeof *frame);
    frame->next = ctx->frames;
    ctx->frames = frame;
    return frame;
}

void drop_frame(PpContext *ctx)
{
    Frame *tmp;

    tmp = ctx->frames;
    if (tmp->type == F_LEXER) {
        if (tmp->prev)
            free_token(tmp->prev);
        lex_free(tmp->lex);
    } else {
        free_tokens(tmp->tokens);
    }
    ctx->frames = ctx->frames->next;
    free(tmp);
}

void pp_push_lex_frame(PpContext *ctx, LexCtx *lex)
{
    Frame *frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->lex = lex;
}

void pp_push_list_frame(PpContext *ctx, Macro *source, Token *tokens)
{
    if (source)
        source->enabled = 0;

    Frame *frame = new_frame(ctx);
    frame->type = F_LIST;
    frame->source = source;
    frame->tokens = tokens;
}

Token *pp_read(PpContext *ctx)
{
    Frame *frame;
    Token *token;

recurse:
    frame = ctx->frames;
    if (!frame)
        return NULL;

    switch (frame->type) {
    case F_LEXER:
        if (frame->prev) {
            // Return saved token
            token = frame->prev;
            frame->prev = NULL;
        } else {
            // Read token directly from lexer
            token = lex_next(frame->lex);
            if (!token) {
                // Remove frame
                if (frame->next == NULL) {
                    // Don't drop bottom lexer frame
                    return NULL;
                }
                drop_frame(ctx);
                goto recurse;
            }
        }
        break;
    case F_LIST:
        // Get token from the token list
        if (!frame->tokens) {
            // Re-enable source macro when popping frame
            if (frame->source)
                frame->source->enabled = 1;
            // Remove frame
            drop_frame(ctx);
            goto recurse;
        }
        // Advance token list
        token = frame->tokens;
        frame->tokens = frame->tokens->next;
        token->next = NULL;
        break;
    }

    return token;
}

Token *pp_peak(PpContext *ctx)
{
    Frame *frame;
    Token *token;

    frame = ctx->frames;
recurse:
    if (!frame)
        return NULL;

    switch (frame->type) {
    case F_LEXER:
        if (frame->prev) {
            // Return saved token
            token = frame->prev;
        } else {
            // Fill frame->prev if it doesn't exist
            token = frame->prev = lex_next(frame->lex);
            if (!token) {
                // Peek at next frame
                frame = frame->next;
                goto recurse;
            }
        }
        break;
    case F_LIST:
        token = frame->tokens;
        if (!token) {
            // Peek at next frame
            frame = frame->next;
            goto recurse;
        }
        break;
    }

    return token;
}

Token *pp_readline(PpContext *ctx)
{
    Token *token;

    token = pp_peak(ctx);
    if (token && !token->flags.lnew)
        return pp_read(ctx);
    return NULL;
}

Macro *new_macro(PpContext *ctx)
{
    Macro *macro = calloc(1, sizeof *macro);
    macro->next = ctx->macros;
    ctx->macros = macro;
    return macro;
}

Macro *find_macro(PpContext *ctx, Token *token)
{
    Macro *macro;

    for (macro = ctx->macros; macro; macro = macro->next) {
        if (!strcmp(macro->name->data, token->data)) {
            return macro;
        }
    }
    return NULL;
}

void free_macro(Macro *macro)
{
    Replace *head, *tmp;

    // Free macro name
    free_token(macro->name);

    // Free replacement list
    head = macro->replace_list;
    while (head) {
        tmp = head->next;
        free_token(head->token);
        free(head);
        head = tmp;
    }

    // Free formal parameters for function like macro
    if (macro->function_like)
        free_tokens(macro->formals);

    free(macro);
}

void del_macro(PpContext *ctx, Token *token)
{
    Macro **macro, *tmp;

    for (macro = &ctx->macros; *macro; macro = &(*macro)->next) {
        // Delete macro if name matches, then return
        if (!strcmp((*macro)->name->data, token->data)) {
            tmp = (*macro)->next;
            free_macro(*macro);
            *macro = tmp;
            return;
        }
    }
}

Cond *new_cond(PpContext *ctx, CondType type)
{
    Cond *cond = calloc(1, sizeof *cond);
    cond->type = type;
    cond->next = ctx->conds;
    ctx->conds = cond;
    return cond;
}

Cond *pop_cond(PpContext *ctx)
{
    Cond *cond = ctx->conds;
    if (cond)
        ctx->conds = cond->next;
    return cond;
}

PpContext *pp_create(void)
{
    PpContext *ctx = calloc(1, sizeof *ctx);
    dirs_init(&ctx->search_dirs);
    return ctx;
}

void pp_free(PpContext *ctx)
{
    dirs_free(&ctx->search_dirs);
    while (ctx->frames) {
        drop_frame(ctx);
    }
    for (Macro *m = ctx->macros; m; ) {
        Macro *next = m->next;
        free_macro(m);
        m = next;
    }
    while (ctx->conds) {
        free(pop_cond(ctx));
    }
    free(ctx);
}

void pp_add_search_dir(PpContext *ctx, const char *dir)
{
    dirs_add(&ctx->search_dirs, dir);
}

int pp_push_file(PpContext *ctx, const char *path)
{
    LexCtx *lex = lex_open_file(path);
    if (!lex)
        return -1;
    pp_push_lex_frame(ctx, lex);
    return 0;
}

void pp_push_string(PpContext *ctx, const char *path, const char *str)
{
    pp_push_lex_frame(ctx, lex_open_string(path, str));
}
