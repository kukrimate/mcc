// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: core logic
//

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <vec.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

static Frame *find_lexer_frame(PpContext *ctx)
{
    // Skip all sub-contexts (they never include lexer frames)
    while (ctx->parent)
        ctx = ctx->parent;
    // Walk the stack of the first real context
    Frame *file_frame = ctx->frames;
    while (file_frame && file_frame->type != F_LEXER)
        file_frame = file_frame->next;
    // A file frame should always exist, otherwise we abort
    if (!file_frame)
        abort();
    return file_frame;
}

static struct tm *find_start_time(PpContext *ctx)
{
    // Only the topmost context has the start time
    while (ctx->parent)
        ctx = ctx->parent;
    return ctx->start_time;
}

void __attribute__((noreturn)) pp_err(PpContext *ctx, const char *err, ...)
{
    Frame *lex_frame = find_lexer_frame(ctx);
    fflush(stdout);
    fprintf(stderr, "Error: %s:%ld: ", lex_path(lex_frame->lex),
        lex_line(lex_frame->lex));
    va_list ap;
    va_start(ap, err);
    vfprintf(stderr, err, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static Token *create_string_lit(const char *str)
{
    StringBuilder sb;
    sb_init(&sb);
    sb_add(&sb, '\"');
    sb_addstr(&sb, str);
    sb_add(&sb, '\"');
    return create_token(TK_STRING_LIT, TOKEN_NOFLAGS, sb_str(&sb));
}

static void handle_date(PpContext *ctx)
{
    char buf[12];
    // FIXME: %b is locale specific :/
    strftime(buf, sizeof buf, "%b %d %Y", find_start_time(ctx));
    token_list_add(pp_push_list_frame(ctx, NULL), create_string_lit(buf));
}

static void handle_time(PpContext *ctx)
{
    char buf[12];
    strftime(buf, sizeof buf, "%H:%M:%S", find_start_time(ctx));
    token_list_add(pp_push_list_frame(ctx, NULL), create_string_lit(buf));
}

static void handle_file(PpContext *ctx)
{
    Frame *lex_frame = find_lexer_frame(ctx);
    // Find filename from path
    const char *prev = lex_path(lex_frame->lex), *next;
    while ((next = strchr(prev, '/')))
        prev = next + 1;
    // Add filename string literal to tokens
    token_list_add(pp_push_list_frame(ctx, NULL), create_string_lit(prev));
}

static void handle_line(PpContext *ctx)
{
    Frame *lex_frame = find_lexer_frame(ctx);
    // Convert line number to string
    char buf[10];
    snprintf(buf, sizeof buf, "%ld", lex_line(lex_frame->lex));
    // Add pre-processing number token with the line number
    token_list_add(pp_push_list_frame(ctx, NULL),
        create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup(buf)));
}

static void handle_vers(PpContext *ctx)
{
    TokenList *list = pp_push_list_frame(ctx, NULL);
    token_list_add(list,
        create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("199901L")));
}

static void handle_one(PpContext *ctx)
{
    TokenList *list = pp_push_list_frame(ctx, NULL);
    token_list_add(list,
        create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("1")));
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

static Frame *new_frame(PpContext *ctx)
{
    Frame *frame = calloc(1, sizeof *frame);
    frame->next = ctx->frames;
    ctx->frames = frame;
    return frame;
}

void pp_push_lex_frame(PpContext *ctx, LexCtx *lex)
{
    Frame *frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->lex = lex;
    cond_list_init(&frame->conds);
}

TokenList *pp_push_list_frame(PpContext *ctx, Macro *source)
{
    Frame *frame = new_frame(ctx);
    frame->type = F_LIST;
    frame->source = source;
    frame->i = 0;

    token_list_init(&frame->list);
    return &frame->list;
}

static void drop_frame(PpContext *ctx)
{
    Frame *frame = ctx->frames;
    if (frame->type == F_LEXER) {
        if (frame->conds.n)
            pp_err(ctx, "Unterminated conditional inclusion");
        // Free lexer context
        lex_free(frame->lex);
        // Free conditional inclusion stack
        cond_list_free(&frame->conds);
    } else {
        // Re-enable macro when popping list frame
        if (frame->source)
            frame->source->enabled = 1;
        // Free any remaining tokens
        for (; frame->i < frame->list.n; ++frame->i)
            free_token(frame->list.arr[frame->i]);
        // Free list
        token_list_free(&frame->list);
    }
    ctx->frames = ctx->frames->next;
    free(frame);
}

Token *pp_read(PpContext *ctx)
{
    Frame *frame;
    Token *token = NULL;

recurse:
    frame = ctx->frames;
    if (frame == NULL)
        return NULL;

    switch (frame->type) {
    case F_LEXER:
        token = lex_next(frame->lex);
        // Drop frame if file has hit its end, and it isn't the bottom frame
        if (token == NULL && frame->next != NULL) {
            drop_frame(ctx);
            goto recurse;
        }
        // Handle pre-processing directives when reading from the lexer
        if (token && token->type == TK_HASH && token->flags.directive) {
            free_token(token);
            handle_directive(ctx);
            goto recurse;
        }
        break;
    case F_LIST:
        // Drop frame if list reached its end
        if (frame->i >= frame->list.n) {
            drop_frame(ctx);
            goto recurse;
        }
        // Get token from the token list
        token = frame->list.arr[frame->i++];
        break;
    }
    return token;
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
    // Free macro name
    free_token(macro->name);

    // Free replacement list
    for (size_t i = 0; i < macro->replace_list.n; ++i)
        free_token(macro->replace_list.arr[i].token);
    replace_list_free(&macro->replace_list);

    // Free formal parameters for function like macro
    if (macro->function_like)
        token_list_freeall(&macro->formals);

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

PpContext *pp_create(void)
{
    PpContext *ctx = calloc(1, sizeof *ctx);
    dirs_init(&ctx->search_dirs);
    time_t rawtime = time(NULL);
    ctx->start_time = localtime(&rawtime);
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
