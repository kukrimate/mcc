// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: macro expansion
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <vec.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

static void capture_actuals(PpContext *ctx, Macro *macro, Token **actuals)
{
    Token *tmp, **tmps;
    size_t actual_cnt, paren_nest;

    // Argument list needs to start with (
    tmp = pp_read(ctx);
    if (!tmp || tmp->type != TK_LEFT_PAREN)
        pp_err(ctx, "Missing ( for function-like macro call");
    free_token(tmp);

    // Just check for closing parenthesis for 0 paramter macro
    if (!macro->param_cnt) {
        tmp = pp_read(ctx);
        if (!tmp || tmp->type != TK_RIGHT_PAREN)
            pp_err(ctx, "Non-empty actual parameters for 0 parameter macro");
        free_token(tmp);
        return;
    }

    // Start of actuals
    actual_cnt = 0;
    tmps = &actuals[actual_cnt++];

    // Start at 1 deep parenthesis
    paren_nest = 1;

    for (;;) {
        tmp = pp_read(ctx);
        if (!tmp)
            pp_err(ctx, "Unexpected end of actual parameters");

        switch (tmp->type) {
        case TK_COMMA:
            // Ignore comma in nested parenthesis
            if (paren_nest > 1 ||
                    (macro->has_varargs && macro->param_cnt == actual_cnt))
                goto add_tok;
            // Free comma
            free_token(tmp);
            // Move to next actual parameter
            if (actual_cnt >= macro->param_cnt)
                pp_err(ctx, "Too many actual parameters");
            tmps = &actuals[actual_cnt++];
            break;
        case TK_LEFT_PAREN:
            // Increase nesting level
            ++paren_nest;
            goto add_tok;
        case TK_RIGHT_PAREN:
            // End of parameters
            if (!--paren_nest)
                goto endloop;
            goto add_tok;
        default:
        add_tok:
            // Add tmp to actual paramter's tmp list
            *tmps = tmp;
            tmps = &(*tmps)->next;
            break;
        }
    }
    endloop:
    // Free right parenthesis
    free_token(tmp);

    if (actual_cnt < macro->param_cnt)
        pp_err(ctx, "Too few actual parameters");
}


static Token *glue_free(Token *l, Token *r)
{
    Token *result;

    result = glue(l, r);
    free_token(l);
    free_token(r);
    return result;
}

Token *pp_next(PpContext *ctx);

static Token *pp_subst(PpContext *ctx, Macro *macro, Token **actuals)
{
    Token     *head, **tail, *tmp, *cur;
    Replace   *replace;
    PpContext subctx;
    _Bool     first, do_glue;

    head = NULL;
    tail = &head;

    do_glue = 0;

    // This is kind of ugly, but duplicating this code
    // woudln't make it any better
    #define glue_tmp                   \
    if (do_glue) {                     \
        *tail = glue_free(*tail, tmp); \
        do_glue = 0;                   \
    } else {                           \
        if (*tail)                     \
            tail = &(*tail)->next;     \
        *tail = tmp;                   \
    }

    for (replace = macro->replace_list; replace; replace = replace->next)
        switch (replace->type) {
        case R_GLUE:
            do_glue = 1;
            break;
        case R_TOKEN:
            tmp = dup_token(replace->token);
            glue_tmp
            break;
        case R_PARAM_EXP:
            if (do_glue) {
                // Shouldn't be possible to reach this, but leaving it
                // just to indicate bugs (which will exist)
                abort();
            }

            // We need to pre-expand the actual parameter
            subctx.parent = ctx;
            subctx.frames = NULL;
            subctx.macros = ctx->macros;
            pp_push_list_frame(&subctx, NULL, dup_tokens(actuals[replace->param_idx]));
            for (first = 1;; first = 0) {
                tmp = pp_next(&subctx);
                if (!tmp)
                    break;
                // Inherit spacing from replacement list
                if (first)
                    tmp->flags.lwhite = replace->token->flags.lwhite;
                // Add token
                if (*tail)
                    tail = &(*tail)->next;
                *tail = tmp;
            }
            break;
        case R_PARAM_STR:
            tmp = stringize(actuals[replace->param_idx]);
            glue_tmp
            break;
        case R_PARAM_GLU:
            if (actuals[replace->param_idx]) {
                // Add all tokens from the actual parameter
                for (cur = actuals[replace->param_idx]; cur; cur = cur->next) {
                    tmp = dup_token(cur);
                    glue_tmp
                }
            } else {
                // Add placemarker if actual parameter is empty
                tmp = create_token(TK_PLACEMARKER, TOKEN_NOFLAGS, NULL);
                glue_tmp
            }
            break;
        }

    #undef glue_tmp
    return head;
}

Token *pp_next(PpContext *ctx)
{
    Token  *identifier, *lparen, *last, **actuals, *expansion;
    Macro  *macro;
    size_t i;

recurse:
    // Read identifier from the frame stack
    identifier = pp_read(ctx);
    if (!identifier)
        return NULL;

    // Ignore placemarkers resulting from previous expansions
    if (identifier->type == TK_PLACEMARKER)
        goto retry;

    // Check for pre-processing directive
    if (identifier->type == TK_HASH && identifier->flags.directive) {
        handle_directive(ctx);
        goto retry;
    }

    // Only identifiers can expand (including pre-defined macros)
    if (identifier->type != TK_IDENTIFIER)
        return identifier;

    // Expand pre-defined macro if requested
    Predef *predef = find_predef(identifier);
    if (predef) {
        predef->handle(ctx);
        goto retry;
    }
    // Check if token has the no expand flag set
    if (identifier->flags.no_expand)
        return identifier;
    // See if the identifier is a macro name
    macro = find_macro(ctx, identifier);
    if (!macro)
        return identifier;

    // If macro is disabled here, this identifier can *never* expand again
    if (!macro->enabled) {
        identifier->flags.no_expand = 1;
        return identifier;
    }

    // We need to capture the actuals if the macro is function like
    if (macro->function_like) {
        // If there are no actuals provided, we don't expand
        lparen = pp_peek(ctx);
        if (!lparen || lparen->type != TK_LEFT_PAREN) {
            // Return identifier
            return identifier;
        }
        // Capture the actuals
        actuals = calloc(macro->param_cnt, sizeof *actuals);
        capture_actuals(ctx, macro, actuals);
        // Expand macro
        expansion = pp_subst(ctx, macro, actuals);
        // Free actuals
        for (i = 0; i < macro->param_cnt; ++i)
            free_tokens(actuals[i]);
        free(actuals);
    } else {
        // Expand macro
        expansion = pp_subst(ctx, macro, NULL);
    }

    // Push result after expansion
    pp_push_list_frame(ctx, macro, expansion);

retry:
    // Next token on the stream inherits our spacing
    if ((last = pp_peek(ctx))) {
        last->flags.lnew = identifier->flags.lnew;
        last->flags.lwhite = identifier->flags.lwhite;
    }
    // Free identifier
    free_token(identifier);
    // Continue recursively expanding
    goto recurse;
}
