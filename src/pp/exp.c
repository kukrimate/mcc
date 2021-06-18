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

static void capture_actuals(PpContext *ctx, Macro *macro, TokenList actuals[])
{
    // Check for closing parenthesis for 0 paramter macro
    if (macro->formals.n == 0) {
        Token *rparen = pp_read(ctx);
        if (!rparen || rparen->type != TK_RIGHT_PAREN)
            pp_err(ctx, "Non-empty actual parameters for 0 parameter macro");
        free_token(rparen);
        return;
    }

    // Start of actuals
    size_t actual_cnt = 1;
    token_list_init(actuals);
    // Start at 1 deep parenthesis
    size_t paren_nest = 1;
    // Translate newline to whitespace per the C standard in macro invocations
    _Bool prev_nl = 0;

    for (;;) {
        Token *tmp = pp_read(ctx);
        if (!tmp)
            pp_err(ctx, "Unexpected end of actual parameters");

        if (prev_nl) {
            prev_nl = 0;
            // tmp->flags.lwhite = 1; FIXME: set this, somehow
        }

        switch (tmp->type) {
        case TK_NEW_LINE:
            // In function like macro invocations newlines are normal whitespaces
            free_token(tmp);
            prev_nl = 1;
            continue;
        case TK_COMMA:
            // Ignore comma in nested parenthesis, or in variadic parameter
            if (paren_nest > 1 || (macro->has_varargs && macro->formals.n == actual_cnt))
                break;
            // Move to next actual parameter
            free_token(tmp);
            if (++actual_cnt > macro->formals.n)
                pp_err(ctx, "Too many actual parameters");
            token_list_init(++actuals);
            continue;
        case TK_LEFT_PAREN:
            // Increase nesting level
            ++paren_nest;
            break;
        case TK_RIGHT_PAREN:
            // Decrease nesting level, add nested paranthesis to list
            if (--paren_nest > 0)
                break;
            // Outer parenthesis means end of actual parameters
            free_token(tmp);
            if (actual_cnt < macro->formals.n)
                pp_err(ctx, "Too few actual parameters");
            return;
        default:
            // Any other token is added to the list
            break;
        }

        token_list_add(actuals, tmp);
    }
}

// Do parameter substitution for a macro replacement list entry
static _Bool expand_replace(PpContext *ctx, Replace *replace, TokenList actuals[], TokenList *expansion)
{
    PpContext subctx = {
        .parent = ctx,
        .macros = ctx->macros,
        .frames = NULL
    };

    _Bool had_tokens = 0;
    switch (replace->type) {
    case R_TOKEN:     // Insert token without substitution
        token_list_add(expansion, ref_token(replace->token));
        return 1;
    case R_PARAM_STR: // Substitute parameter as is
        token_list_add(expansion, stringize_operator(actuals + replace->param_idx));
        return 1;
    case R_PARAM_GLU: // Substitute parameter as is
        token_list_refxtend(expansion, actuals + replace->param_idx);
        return actuals[replace->param_idx].n > 0;
    case R_PARAM_EXP: // Substitute pre-expanded parameter
        token_list_refxtend(pp_push_list_frame(&subctx, NULL), actuals + replace->param_idx);
        for (Token *token; (token = pp_next(&subctx)); ) {
            token_list_add(expansion, token);
            had_tokens = 1;
        }
        break;
    }
    return had_tokens;
}

// Do parameter substitution for the full replacement list of a macro, also evaluate ## operators
static void expand_macro(PpContext *ctx, Macro *macro, TokenList actuals[], TokenList *expansion)
{
    for (Replace *replace = macro->replace_list; replace; replace = replace->next) {
        // Evaluate ## operators left to right
        _Bool had_left = expand_replace(ctx, replace, actuals, expansion);
        while (replace->glue_next) {
            // Make sure the right hand operand actually there
            assert((replace = replace->next) != NULL);
            if (had_left) {
                // Save the last token of the left side
                Token *left = token_list_pop(expansion);
                // Save the index of where to write the ## result
                size_t result_idx = expansion->n;
                if (expand_replace(ctx, replace, actuals, expansion)) {
                    // Replace the first right token token with the glue result
                    expansion->arr[result_idx] = glue_operator(left, expansion->arr[result_idx]);
                } else {
                    // No right tokens -> glue result is the last left token
                    token_list_add(expansion, left);
                }
            } else {
                // No left tokens -> glue result is the first right token
                had_left = expand_replace(ctx, replace, actuals, expansion);
            }
        }
    }
}

// Look for a ( token after an arbitrary number of newlines
static _Bool match_lparen(PpContext *ctx)
{
    TokenList list;
    token_list_init(&list);
    for (;;) {
        Token *token = pp_read(ctx);
        if (token) {
            token_list_add(&list, token);
            if (token->type == TK_LEFT_PAREN) {
                token_list_freeall(&list);
                return 1;
            }
            if (token->type != TK_NEW_LINE)
                break;
        } else {
            break;
        }
    }
    token_list_extend(pp_push_list_frame(ctx, NULL), &list);
    token_list_free(&list);
    return 0;
}

static _Bool try_expand(PpContext *ctx, Macro *macro)
{
    if (macro->function_like) {
        // Function like macro name must be followed by a ( token
        if (!match_lparen(ctx))
            return 0;
        // Capture the actuals
        TokenList *actuals = calloc(macro->formals.n, sizeof *actuals);
        capture_actuals(ctx, macro, actuals);
        // Expand macro
        expand_macro(ctx, macro, actuals, pp_push_list_frame(ctx, macro));
        // Free actuals
        for (size_t i = 0; i < macro->formals.n; ++i)
            token_list_freeall(actuals + i);
        free(actuals);
    } else {
        expand_macro(ctx, macro, NULL, pp_push_list_frame(ctx, macro));
    }
    // This macro can't expand again until the expansion frame is dropped
    macro->enabled = 0;
    return 1;
}

Token *pp_next(PpContext *ctx)
{
    Predef *predef;
    Macro *macro;

    for (Token *token;; free_token(token)) {
        token = pp_read(ctx);
        if (token == NULL)
            return NULL;

        switch (token->type) {
        case TK_IDENTIFIER:
            // Always expand pre-defined macro
            if ((predef = find_predef(token))) {
                predef->handle(ctx);
                continue;
            }
            // Try expanding macro if token is available for expansion
            if (!token->flags.no_expand && (macro = find_macro(ctx, token))) {
                if (macro->enabled) {
                    // Try expanding macro
                    if (try_expand(ctx, macro))
                        continue;
                } else {
                    // Mark the token unavailable for expansion in the future
                    token->flags.no_expand = 1;
                }
            }
            return token;
        case TK_HASH:
            // Handle pre-processing directive if appropriate
            if (token->flags.directive) {
                handle_directive(ctx);
                continue;
            }
            return token;
        default:
            return token;
        }
    }
}
