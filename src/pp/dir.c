// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: directive handling
//

#include <stdio.h>
#include <limits.h>
#include <vec.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

static ssize_t find_formal(Macro *macro, Token *token)
{
    size_t idx;
    Token *formal;

    if (token->type != TK_IDENTIFIER)
        return -1;

    idx = 0;
    for (formal = macro->formals; formal; formal = formal->next) {
        if (!strcmp(formal->data, token->data))
            return idx;
        ++idx;
    }
    return -1;
}

static void capture_formals(PpContext *ctx, Macro *macro)
{
    Token **tail = &macro->formals, *token;

    macro->param_cnt = 0;
    macro->has_varargs = 0;

want_identifier:
    // Read formal parameter name
    token = pp_readline(ctx);
    if (!token)
        pp_err(ctx, "Unexpected end of formal parameters");
    if (token->type == TK_RIGHT_PAREN)
        goto end;
    // Variable argument mode
    if (token->type == TK_VARARGS) {
        free_token(token);
        token = create_token(TK_IDENTIFIER, TOKEN_NOFLAGS, strdup("__VA_ARGS__"));
        macro->has_varargs = 1;
    }
    // Must be an identifier
    if (token->type != TK_IDENTIFIER)
        pp_err(ctx, "Invalid token in formal parameter list");

    // Make sure it's not duplicate
    if (find_formal(macro, token) >= 0)
        pp_err(ctx, "Duplicate formal parameter name");

    // Add name to the list
    *tail = token;
    tail = &(*tail)->next;
    ++macro->param_cnt;

    // Next token must be a , or )
    token = pp_readline(ctx);
    if (!token)
        pp_err(ctx, "Unexpected end of formal parameters");
    if (token->type == TK_COMMA) {
        if (macro->has_varargs)
            pp_err(ctx, "Variable args must be the last formal parameter of macro");
        free_token(token);
        goto want_identifier;
    }
    if (token->type != TK_RIGHT_PAREN)
        pp_err(ctx, "Invalid token in formal parameter list");
end:
    free_token(token);
}

static void capture_replace_list(Token *token, PpContext *ctx, Macro *macro)
{
    Replace *prev, **tail;
    ssize_t formal_idx;
    _Bool   do_glue;

    prev = NULL;
    tail = &macro->replace_list;
    do_glue = 0;

    for (; token; token = pp_readline(ctx)) {
        switch (token->type) {
        case TK_HASH_HASH:
            // Must be preceeded by something
            if (!macro->replace_list)
                pp_err(ctx, "## operator must not be the first token of a replacement list");

            // Do *not* expand parameter before ## operator
            if (prev->type == R_PARAM_EXP)
                prev->type = R_PARAM_GLU;

            // Wait for the right-hand-side of glue
            do_glue = 1;

            // Add a glue operator to the list
            prev = *tail = calloc(1, sizeof **tail);
            (*tail)->type = R_GLUE;
            (*tail)->token = token;
            tail = &(*tail)->next;
            break;
        case TK_HASH:
            if (macro->function_like) {
                // Free hash
                free_token(token);
                // Get formal parameter name
                token = pp_readline(ctx);
                if (!token || (formal_idx = find_formal(macro, token)) < 0) {
                    pp_err(ctx, "# operator must be followed by formal parameter name");
                }

                prev = *tail = calloc(1, sizeof **tail);
                (*tail)->type = R_PARAM_STR;
                (*tail)->token = token;
                (*tail)->param_idx = formal_idx;
                tail = &(*tail)->next;
                do_glue = 0;
                break;
            }
            // FALLTHROUGH
        default:
            // Append token or parameter index to replacement list
            prev = *tail = calloc(1, sizeof **tail);
            if (!macro->function_like || (formal_idx = find_formal(macro, token)) < 0) {
                (*tail)->type = R_TOKEN;
                (*tail)->token = token;
            } else {
                (*tail)->type  = do_glue ? R_PARAM_GLU : R_PARAM_EXP;
                (*tail)->token = token;
                (*tail)->param_idx = formal_idx;
            }
            tail = &(*tail)->next;
            do_glue = 0;
            break;
        }
    }

    if (do_glue)
        pp_err(ctx, "## operator must not be the last token in a replacement list");
}

static void dir_define(PpContext *ctx)
{
    Token *token;
    Macro *macro;

    // Macro name must be an identifier
    token = pp_readline(ctx);
    if (!token || token->type != TK_IDENTIFIER)
        pp_err(ctx, "Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    macro = new_macro(ctx);
    macro->name = token;
    macro->enabled = 1;

    // Check for macro type
    token = pp_readline(ctx);
    if (token && token->type == TK_LEFT_PAREN
            && !token->flags.lnew && !token->flags.lwhite) {
        // Free left parenthesis
        free_token(token);
        // Function like macro
        macro->function_like = 1;
        capture_formals(ctx, macro);
        // Capture replacement list
        capture_replace_list(pp_readline(ctx), ctx, macro);
    } else {
        // Object-like macro
        macro->function_like = 0;
        // Capture replacement list
        capture_replace_list(token, ctx, macro);
    }

}

static void dir_undef(PpContext *ctx)
{
    Token *tmp;

    // Macro name must be an identifier
    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        pp_err(ctx, "Macro name must be an identifier");
    // Delete macro
    del_macro(ctx, tmp);
    free_token(tmp);
}

static _Bool is_cexpr(PpContext *ctx)
{
    Token     *head, **tail, *tmp;
    _Bool     want_paren, result;
    PpContext subctx;

    // Capture constant expression
    head = NULL;
    tail = &head;

    while ((*tail = pp_readline(ctx))) {
        if ((*tail)->type == TK_IDENTIFIER
                && !strcmp((*tail)->data, "defined")) {
            free_token(*tail);
            // Check for left parenthesis
            if (!(*tail = pp_readline(ctx)))
                goto err_defined;
            want_paren = (*tail)->type == TK_LEFT_PAREN;
            if (want_paren) {
                free_token(*tail);
                if (!(*tail = pp_readline(ctx)))
                    goto err_defined;
            }

            // Check macro name
            if ((*tail)->type != TK_IDENTIFIER)
                goto err_defined;

            // Replace macro name with number
            result = find_predef(*tail) || find_macro(ctx, *tail);
            free_token(*tail);
            if (result)
                *tail = create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("1"));
            else
                *tail = create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("0"));

            // Make sure we have right parenthesis if needed
            if (want_paren) {
                tmp = pp_readline(ctx);
                if (!tmp || tmp->type != TK_RIGHT_PAREN)
                    goto err_defined;
                free_token(tmp);
            }
        }
        tail = &(*tail)->next;
    }

    // We need to macro expand the constant expression
    subctx.parent = ctx;
    subctx.frames = NULL;
    subctx.macros = ctx->macros;
    pp_push_list_frame(&subctx, NULL, head);

    head = NULL;
    tail = &head;
    while ((*tail = pp_next(&subctx)))
        tail = &(*tail)->next;

    // Finally evaluate the expression
    result = eval_cexpr(head);
    free_tokens(head);
    return result;

err_defined:
    pp_err(ctx, "Missing/malformed argument for defined operator");
}

static _Bool is_defined(PpContext *ctx)
{
    Token *tmp;
    _Bool result;

    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        pp_err(ctx, "#if(n)def must be followed by a macro name");

    result = find_predef(tmp) || find_macro(ctx, tmp);
    free_token(tmp);
    return result;
}

// Skip till the next signficant conditional, if want_else_elif it can be either
// an elif, else, or endif, if not want_else_elif it can only be an endif
static CondType skip_cond(PpContext *ctx, _Bool want_else_elif)
{
    size_t nest;
    Token  *tmp;

    nest = 1;
    while (nest) {
        tmp = pp_read(ctx);
        if (!tmp)
            goto err;

        // Nested directive
        if (tmp->type == TK_HASH && tmp->flags.directive) {
            // Read directive name
            free_token(tmp);
            tmp = pp_read(ctx);
            if (!tmp)
                goto err;

            // We don't care about empty or invalid directives here
            if (tmp->flags.lnew || tmp->type != TK_IDENTIFIER)
                continue;

            // Check for nested #else or #elif if at the correct nesting level
            // and it is desired
            if (nest == 1 && want_else_elif) {
                if (!strcmp("else", tmp->data)) {
                    free_token(tmp);
                    return C_ELSE;
                }

                if (!strcmp("elif", tmp->data)) {
                    free_token(tmp);
                    return C_ELIF;
                }
            }

            // Check for nested #if directive
            if (!strcmp("if", tmp->data)
                    || !strcmp("ifdef", tmp->data)
                    || !strcmp("ifndef", tmp->data))
                ++nest;
            else if (!strcmp("endif", tmp->data))
                --nest;
        }

        free_token(tmp);
    }

    // We got an #endif
    return C_ENDIF;

err:
    pp_err(ctx, "Unterminated conditional inclusion");
}

// Handle #if directive
static void dir_if(PpContext *ctx, _Bool eval, CondType type)
{
    Cond *cond = new_cond(ctx, type);
again:
    if (!eval)
        switch (skip_cond(ctx, 1)) {
        default:      // Not possible
            abort();
        case C_ELSE:  // #else of skipped if always gets executed
            cond->type = C_ELSE;
            return;
        case C_ELIF:  // Re-test condition on #elif
            cond->type = C_ELIF;
            eval = is_cexpr(ctx);
            goto again;
        case C_ENDIF: // #endif just pops the current conditional
            free(pop_cond(ctx));
            return;
        }
}

static void dir_else(PpContext *ctx)
{
    // #else or #elif must come after an #if or #elif
    Cond *prev = pop_cond(ctx);
    if (!prev || !(prev->type == C_IF || prev->type == C_ELIF))
        pp_err(ctx, "Unexpected #else or #elif");
    free(prev);

    // #else or #elif of an active #if just skips till #endif
    skip_cond(ctx, 0);
}

static void dir_endif(PpContext *ctx)
{
    Cond *cond;

    // #endif must be preceded by some other conditional
    cond = pop_cond(ctx);
    if (!cond)
        pp_err(ctx, "Unexpected #endif");
    free(cond);
}

static LexCtx *open_system_header(PpContext *ctx, const char *name)
{
    char path[PATH_MAX];
    LexCtx *lex;

    for (size_t i = 0; i < ctx->search_dirs.n; ++i) {
        snprintf(path, sizeof path, "%s/%s", ctx->search_dirs.arr[i], name);
        if ((lex = lex_open_file(path)))
            return lex;
    }

    return NULL;
}

static LexCtx *open_local_header(PpContext *ctx, const char *name)
{
    LexCtx *lex;

    // Retry failed local header as a system one
    if (!(lex = lex_open_file(name)))
        return open_system_header(ctx, name);
    return lex;
}

static char *read_hchar(PpContext *ctx)
{
    Token *head = NULL;

    for (Token **tail = &head;; tail = &(*tail)->next) {
        *tail = pp_readline(ctx);
        if (*tail == NULL)
            return NULL;
        if ((*tail)->type == TK_RIGHT_ANGLE) {
            free_token(*tail);
            *tail = NULL;
            break;
        }
    }
    char *hchar_str = concat_spellings(head);
    free_tokens(head);
    return hchar_str;
}

static char *read_qchar(Token *token)
{
    size_t len = strlen(token->data);
    if (len < 2 || token->data[0] != '\"' || token->data[len - 1] != '\"')
        return NULL;
    return strndup(token->data + 1, len - 2);
}

// #include directive
static void dir_include(PpContext *ctx)
{
    Token *token = pp_readline(ctx);
    if (!token)
        goto err_invalid;

    char *name;
    LexCtx *lex;

    switch (token->type) {
    case TK_LEFT_ANGLE:
        name = read_hchar(ctx);
        if (name == NULL)
            goto err_invalid;
        lex = open_system_header(ctx, name);
        free(name);
        break;
    case TK_STRING_LIT:
        name = read_qchar(token);
        if (name == NULL)
            goto err_invalid;
        lex = open_local_header(ctx, name);
        free(name);
        break;
    default:
        goto err_invalid;
    }
    free_token(token);
    if (!lex)
        pp_err(ctx, "Can't locate header file: %s", name);
    pp_push_lex_frame(ctx, lex);
    return;

err_invalid:
    pp_err(ctx, "Invalid header name");
}

void handle_directive(PpContext *ctx)
{
    Token *tmp;

    tmp = pp_readline(ctx);
    // Check for empty directive
    if (!tmp)
        return;

    // Otherwise the directive name must follow
    if (tmp->type != TK_IDENTIFIER)
        pp_err(ctx, "Pre-processing directive must be an identifier");

    // Check for all supported directives
    if (!strcmp(tmp->data, "define"))
        dir_define(ctx);
    else if (!strcmp(tmp->data, "undef"))
        dir_undef(ctx);
    else if (!strcmp(tmp->data, "if"))
        dir_if(ctx, is_cexpr(ctx), C_IF);
    else if (!strcmp(tmp->data, "ifdef"))
        dir_if(ctx, is_defined(ctx), C_IF);
    else if (!strcmp(tmp->data, "ifndef"))
        dir_if(ctx, !is_defined(ctx), C_IF);
    else if (!strcmp(tmp->data, "elif"))
        dir_else(ctx);
    else if (!strcmp(tmp->data, "else"))
        dir_else(ctx);
    else if (!strcmp(tmp->data, "endif"))
        dir_endif(ctx);
    else if (!strcmp(tmp->data, "include"))
        dir_include(ctx);
    else
        pp_err(ctx, "Unknown pre-prerocessing directive");

    // Free directive name
    free_token(tmp);
}
