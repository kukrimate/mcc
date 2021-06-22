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

// Capture the actual parameters for a function like macro call
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
    // Was the previous token a newline
    _Bool prev_nl = 0;

    for (;;) {
        Token *token = pp_read(ctx);
        if (!token)
            pp_err(ctx, "Unexpected end of actual parameters");
        if (prev_nl)
            token->flags.lwhite = 1;

        switch (token->type) {
        case TK_NEW_LINE:
            // Translate newlines to whitespaces in macro invocations
            prev_nl = 1;
            free_token(token);
            break;
        case TK_COMMA:
            // Ignore comma in nested parenthesis, or in variadic parameter
            if (paren_nest > 1
                    || (macro->has_varargs && macro->formals.n == actual_cnt))
                break;
            // Move to next actual parameter
            free_token(token);
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
            free_token(token);
            if (actual_cnt < macro->formals.n)
                pp_err(ctx, "Too few actual parameters");
            return;
        default:
            // Any other token is added to the list
            break;
        }

        token_list_add(actuals, token);
    }
}

// Create a string literal with the spellings of a list of tokens
Token *stringize(_Bool lit_lwhite, TokenList *tokens)
{
    StringBuilder sb;
    sb_init(&sb);
    sb_add(&sb, '\"');
    for (size_t i = 0; i < tokens->n; ++i) {
        Token *token = tokens->arr[i];
        // Add whitespace if any
        if (i > 0 && token->flags.lwhite)
            sb_add(&sb, ' ');
        // Add token spelling
        switch (token->type) {
        case TK_CHAR_CONST:
        case TK_STRING_LIT:
            for (const char *s = token->data; *s; ++s)
                switch (*s) {
                case '\\':
                case '\"':
                    sb_add(&sb, '\\');
                    // FALLTHROUGH
                default:
                    sb_add(&sb, *s);
                    break;
                }
            break;
        default:
            sb_addstr(&sb, token_spelling(token));
            break;
        }
    }
    sb_add(&sb, '\"');
    return create_token(TK_STRING_LIT,
        (TokenFlags) { .lwhite = lit_lwhite }, sb_str(&sb));
}

// Evaluate a single replacement list entry
static _Bool expand_replace(PpContext *ctx, Replace *replace,
    TokenList actuals[], TokenList *expansion)
{
    if (replace->type == R_TOKEN) {
        token_list_add(expansion, dup_token(replace->token));
        return 1;
    } else if (replace->type == R_OP_STR) {
        token_list_add(expansion, stringize(replace->token->flags.lwhite,
            actuals + replace->param_idx));
        return 1;
    } else if (replace->type == R_OP_GLU) {
        _Bool had_tokens = 0;
        for (size_t i = 0; i < actuals[replace->param_idx].n; ++i) {
            Token *token = dup_token(actuals[replace->param_idx].arr[i]);
            if (!had_tokens) {
                token->flags.lwhite = replace->token->flags.lwhite;
                had_tokens = 1;
            }
            token_list_add(expansion, token);
        }
        return had_tokens;
    } else {
        PpContext subctx = { .parent = ctx, .macros = ctx->macros, .frames = NULL };
        TokenList *input = pp_push_list_frame(&subctx, NULL);
        for (size_t i = 0; i < actuals[replace->param_idx].n; ++i)
            token_list_add(input, dup_token(actuals[replace->param_idx].arr[i]));
        _Bool had_tokens = 0;
        for (Token *token; (token = pp_next(&subctx)); ) {
            if (!had_tokens) {
                token->flags.lwhite = replace->token->flags.lwhite;
                had_tokens = 1;
            }
            token_list_add(expansion, token);
        }
        return had_tokens;
    }
}

Token *glue(Token *left, Token *right)
{
    // Combine the spelling of the two tokens (without whitespaces)
    StringBuilder sb;
    sb_init(&sb);
    sb_addstr(&sb, token_spelling(left));
    sb_addstr(&sb, token_spelling(right));
    char *combined = sb_str(&sb);
    // Re-lex new combined token
    LexCtx *lex = lex_open_string("glue_tmp", combined);
    Token *result = lex_next(lex);
    result->flags.lwhite = left->flags.lwhite;
    // If there are more tokens, it means glue failed
    if (lex_next(lex))
        return NULL;
    lex_free(lex);
    free(combined);
    free_token(left);
    free_token(right);
    return result;
}

// Macro parameter substitution, including ## evaluation
static void expand_macro(PpContext *ctx, Macro *macro, TokenList actuals[], TokenList *expansion)
{
    Replace *replace = macro->replace_list.arr;
    for (size_t i = 0; i < macro->replace_list.n; ++i, ++replace) {
        // Evaluate ## operators left to right
        if (expand_replace(ctx, replace, actuals, expansion))
            while (replace->glue_next) {
                // Make sure the right hand operand actually there
                assert(++i < macro->replace_list.n);
                ++replace;

                // Find last non-pad token on the left
                Token *left = token_list_pop(expansion);
                // Save the index of where to write the ## result
                size_t result_idx = expansion->n;

                if (expand_replace(ctx, replace, actuals, expansion)) {
                    // Replace the first right token token with the glue result
                    expansion->arr[result_idx] = glue(left, expansion->arr[result_idx]);
                    if (expansion->arr[result_idx] == NULL)
                        pp_err(ctx, "Token concatenation resulted in more than one token");
                } else {
                    // No right tokens -> glue result is the last left token
                    token_list_add(expansion, left);
                }
            }
    }
}

static _Bool try_expand(PpContext *ctx, Token *identifier, Macro *macro)
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

    // First token from expansion (or next on the stream if empty) inherits
    // the identifier's spacing
    Token *token = pp_read(ctx);
    if (token) {
        token->flags.lwhite = identifier->flags.lwhite;
        token_list_add(pp_push_list_frame(ctx, NULL), token);
    }

    return 1;
}

Token *pp_next(PpContext *ctx)
{
    Predef *predef;
    Macro *macro;

    for (Token *token;; free_token(token)) {
        token = pp_read(ctx);
        if (token && token->type == TK_IDENTIFIER) {
            // Always expand pre-defined macro
            if ((predef = find_predef(token))) {
                predef->handle(ctx);
                continue;
            }
            // Try expanding macro if token is available for expansion
            if (!token->flags.no_expand && (macro = find_macro(ctx, token))) {
                if (macro->enabled) {
                    // Try expanding macro
                    if (try_expand(ctx, token, macro))
                        continue;
                } else {
                    // Mark the token unavailable for expansion in the future
                    token->flags.no_expand = 1;
                }
            }
        }
        return token;
    }
}
