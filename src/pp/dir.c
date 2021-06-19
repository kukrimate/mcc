// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: directive handling
//

#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <vec.h>
#include <lex/token.h>
#include <lex/lex.h>
#include "pp.h"
#include "def.h"

// Read from the current pre-processor frame's underlying lexer context
static Token *dir_read(PpContext *ctx)
{
    assert(ctx->frames && ctx->frames->type == F_LEXER);
    return lex_next(ctx->frames->lex);
}

static void push_cond(PpContext *ctx, Cond cond)
{
    assert(ctx->frames && ctx->frames->type == F_LEXER);
    cond_list_add(&ctx->frames->conds, cond);
}

static int pop_cond(PpContext *ctx)
{
    assert(ctx->frames && ctx->frames->type == F_LEXER);
    if (ctx->frames->conds.n > 0)
        return cond_list_pop(&ctx->frames->conds);
    return -1;
}

// Make sure there are no stray tokens left before the terminating newline of
// a pre-processing directive
static void dir_expect_newline(PpContext *ctx)
{
    Token *token = dir_read(ctx);
    if (token) {
        if (token->type != TK_NEW_LINE)
            pp_err(ctx, "Missing newline after pre-processing directive");
        free_token(token);
    }
}

// Find the index of macro's formal parameter by name
static ssize_t find_formal(Macro *macro, Token *token)
{
    if (token->type == TK_IDENTIFIER) {
        for (size_t i = 0; i < macro->formals.n; ++i) {
            if (!strcmp(macro->formals.arr[i]->data, token->data))
                return i;
        }
    }
    return -1;
}

static Token *capture_formals_read(PpContext *ctx)
{
    Token *token = dir_read(ctx);
    if (!token)
        pp_err(ctx, "Unexpected end of formal parameters");
    if (token->type == TK_NEW_LINE)
        pp_err(ctx, "Newline in formal parameter list");
    return token;
}

static void capture_formals(PpContext *ctx, Macro *macro)
{
    macro->has_varargs = 0;
    token_list_init(&macro->formals);

    // If the first token is ), it's a 0 parameter macro
    Token *token = capture_formals_read(ctx);
    if (token->type == TK_RIGHT_PAREN) {
        free_token(token);
        return;
    }

    // Build list of formal parameter
    for (;; token = capture_formals_read(ctx)) {
        switch (token->type) {
        default:
            pp_err(ctx, "Invalid token in formal parameter list");
        case TK_VARARGS:
            // Mark macro as variadic
            macro->has_varargs = 1;
            // Replace ... token with the variadic argument marker
            free_token(token);
            token = create_token(TK_IDENTIFIER, TOKEN_NOFLAGS,
                strdup("__VA_ARGS__"));
            break;
        case TK_IDENTIFIER:
            // Make sure __VA_ARGS__ is not used as formal parameter name
            if (!strcmp(token->data, "__VA_ARGS__"))
                pp_err(ctx, "__VA_ARGS__ used as a formal parameter name");
            // Make sure formal parameter name is not a duplicate
            if (find_formal(macro, token) >= 0)
                pp_err(ctx, "Duplicate formal parameter name");
            break;
        }

        // Add token to formal parameter list
        token_list_add(&macro->formals, token);

        // Next token must be either , or )
        token = capture_formals_read(ctx);
        switch (token->type) {
        default:
            pp_err(ctx, "Invalid token in formal parameter list");
        case TK_COMMA:
            free_token(token);
            continue;
        case TK_RIGHT_PAREN:
            free_token(token);
            return;
        }
    }
}

static void capture_replace_list(Token *token, PpContext *ctx, Macro *macro)
{
    Replace *prev = NULL, **tail = &macro->replace_list;
    _Bool need_glue_rhs = 0;
    ssize_t formal_idx;

    for (;; token = dir_read(ctx)) {
        if (!token)
            pp_err(ctx, "Replacement list must be terminated by a newline");
        switch (token->type) {
        case TK_HASH_HASH:
            // Free ## token
            free_token(token);
            // Must be preceeded by something
            if (!prev)
                pp_err(ctx, "## operator must not be the first token of a replacement list");
            // Glue the previous entry to the next
            if (prev->type == R_PARAM_EXP)
                prev->type = R_PARAM_GLU;
            prev->glue_next = 1;
            // Make sure the RHS actually exists
            need_glue_rhs = 1;
            break;
        case TK_HASH:
            if (macro->function_like) {
                // Free # token
                free_token(token);
                // Get formal parameter name
                token = dir_read(ctx);
                if (!token || (formal_idx = find_formal(macro, token)) < 0)
                    pp_err(ctx, "# operator must be followed by formal parameter name");
                // Stringized parameter will be added to the expansion
                prev = *tail = calloc(1, sizeof **tail);
                (*tail)->type = R_PARAM_STR;
                (*tail)->token = token;
                (*tail)->param_idx = formal_idx;
                tail = &(*tail)->next;
                need_glue_rhs = 0;
                break;
            }
            // FALLTHROUGH
        default:
            // Append token or parameter index to replacement list
            prev = *tail = calloc(1, sizeof **tail);
            if (macro->function_like && (formal_idx = find_formal(macro, token)) >= 0) {
                (*tail)->type = need_glue_rhs ? R_PARAM_GLU : R_PARAM_EXP;
                (*tail)->token = token;
                (*tail)->param_idx = formal_idx;
            } else {
                (*tail)->type = R_TOKEN;
                (*tail)->token = token;
            }
            tail = &(*tail)->next;
            need_glue_rhs = 0;
            break;
        case TK_NEW_LINE:
            if (need_glue_rhs)
                pp_err(ctx, "## operator must not be the last token in a replacement list");
            free_token(token);
            return;
        }
    }
}

static void dir_define(PpContext *ctx)
{
    // Macro name must be an identifier
    Token *token = dir_read(ctx);
    if (!token || token->type != TK_IDENTIFIER)
        pp_err(ctx, "Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    Macro *macro = new_macro(ctx);
    macro->name = token;
    macro->enabled = 1;

    // Check for macro type
    token = dir_read(ctx);
    if (token && token->type == TK_LEFT_PAREN && !token->flags.lwhite) {
        // Free left parenthesis
        free_token(token);
        // Function like macro
        macro->function_like = 1;
        capture_formals(ctx, macro);
        // Capture replacement list
        capture_replace_list(dir_read(ctx), ctx, macro);
    } else {
        // Object-like macro
        macro->function_like = 0;
        // Capture replacement list
        capture_replace_list(token, ctx, macro);
    }

    // NOTE: capture_replace_list already consumed the terminating newline
}

static void dir_undef(PpContext *ctx)
{
    // Macro name must be an identifier
    Token *token = dir_read(ctx);
    if (!token || token->type != TK_IDENTIFIER)
        pp_err(ctx, "Macro name must be an identifier");

    // Delete macro
    del_macro(ctx, token);
    free_token(token);

    // Must end with a newline
    dir_expect_newline(ctx);
}

static Token *defined_operator(PpContext *ctx)
{
    Token *token = dir_read(ctx);
    if (!token)
        goto err;

    // Handle the form: defined ( IDENTIFIER )
    if (token->type == TK_LEFT_PAREN) {
        free_token(token);
        if (!(token = dir_read(ctx)))
            goto err;
        Token *rparen = dir_read(ctx);
        if (!rparen || rparen->type != TK_RIGHT_PAREN)
            goto err;
        free_token(rparen);
    }

    // Make sure the macro name is actually an identifier
    if (token->type != TK_IDENTIFIER)
        goto err;

    // Replace macro name with number
    _Bool macro_defined = find_predef(token) || find_macro(ctx, token);
    free_token(token);
    if (macro_defined)
        return create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("1"));
    else
        return create_token(TK_PP_NUMBER, TOKEN_NOFLAGS, strdup("0"));
err:
    pp_err(ctx, "Missing/malformed argument for defined operator");
}

static _Bool eval_if(PpContext *ctx)
{
    PpContext subctx = { .parent = ctx, .frames = NULL, .macros = ctx->macros };
    TokenList *list = pp_push_list_frame(&subctx, NULL);

    // Capture constant expression, evaluating the defined operator
    for (;;) {
        Token *token = dir_read(ctx);
        if (!token)
            pp_err(ctx, "#if missing terminating newline");
        if (token->type == TK_NEW_LINE) {
            free_token(token);
            break;
        }
        if (token->type == TK_IDENTIFIER && !strcmp(token->data, "defined")) {
            free_token(token);
            token = defined_operator(ctx);
        }
        token_list_add(list, token);
    }

    // Macro expand constant expression
    TokenList cexpr;
    token_list_init(&cexpr);
    for (Token *token; (token = pp_next(&subctx)); )
        token_list_add(&cexpr, token);

    // Finally evaluate the constant expression
    _Bool result = eval_cexpr(&cexpr);
    token_list_freeall(&cexpr);
    return result;
}

static _Bool eval_ifdef(PpContext *ctx)
{
    // Macro name must be an identifier
    Token *token = dir_read(ctx);
    if (!token || token->type != TK_IDENTIFIER)
        pp_err(ctx, "#if(n)def must be followed by a macro name");

    // Check if macro name was defined
    _Bool macro_defined = find_predef(token) || find_macro(ctx, token);
    free_token(token);

    // Must end with a newline
    dir_expect_newline(ctx);

    return macro_defined;
}

// Find the end or alternative branch of non-evaluated conditional
static Cond skip_cond(PpContext *ctx, _Bool want_else_elif)
{
    for (size_t nest = 1; nest; ) {
        Token *token = dir_read(ctx);
        if (!token)
            pp_err(ctx, "Unterminated conditional inclusion");

        // Look for nested #if
        if (token->type == TK_HASH && token->flags.directive) {
            free_token(token);

            // Read directive name, skipping empty or invalid directives
            token = dir_read(ctx);
            if (!token || token->type != TK_IDENTIFIER)
                continue;

            // Check for alternative branch of the outer conditional if requested
            if (want_else_elif && nest == 1) {
                if (!strcmp("else", token->data)) {
                    free_token(token);
                    return C_ELSE;
                }

                if (!strcmp("elif", token->data)) {
                    free_token(token);
                    return C_ELIF;
                }
            }

            // Check for nested #if directive
            if (!strcmp("if", token->data)
                    || !strcmp("ifdef", token->data)
                    || !strcmp("ifndef", token->data))
                ++nest;
            else if (!strcmp("endif", token->data))
                --nest;
        }

        free_token(token);
    }

    // Nesting level reacing 0 means #endif
    return C_ENDIF;
}

// Handle #if/#ifdef/#ifndef directives
static void dir_if(PpContext *ctx, _Bool condition)
{
    // Evaluate #if/#ifdef/#ifndef if condition is true
    if (condition) {
        push_cond(ctx, C_IF);
        // NOTE: eval_if or eval_ifdef already consumed the newline
        return;
    }

    // Look for alternative branch of non-evaluated conditional
    for (;;)
        switch (skip_cond(ctx, 1)) {
        case C_IF:          // Not reached
            abort();
        case C_ELIF:        // #elif: evaluate only if condition is true
            if (eval_if(ctx)) {
                push_cond(ctx, C_ELIF);
                return;
            }
            break;
        case C_ELSE:        // #else: always evaluate
            push_cond(ctx, C_ELSE);
            dir_expect_newline(ctx);
            return;
        case C_ENDIF:       // #endif: nothing to evaluate
            dir_expect_newline(ctx);
            return;
        }
}

// Handle #elif/#else directives
static void dir_else(PpContext *ctx)
{
    // #else or #elif must come after an #if or #elif
    int prev = pop_cond(ctx);
    if (prev != C_IF && prev != C_ELIF)
        pp_err(ctx, "Unexpected #else or #elif");
    // #else or #elif of an evaluated #if just skips till #endif
    skip_cond(ctx, 0);
    // Must end with a newline
    dir_expect_newline(ctx);
}

static void dir_endif(PpContext *ctx)
{
    // #endif must be preceded by some other conditional
    if (pop_cond(ctx) < 0)
        pp_err(ctx, "Unexpected #endif");
    // Must end with a newline
    dir_expect_newline(ctx);
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
    TokenList list;
    token_list_init(&list);

    for (;;) {
        Token *token = dir_read(ctx);
        if (!token) {
            token_list_freeall(&list);
            return NULL;
        }
        if (token->type == TK_RIGHT_ANGLE) {
            free_token(token);
            break;
        }
        token_list_add(&list, token);
    }
    char *hchar_str = concat_spellings(&list);
    token_list_freeall(&list);
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
    Token *token = dir_read(ctx);
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
        break;
    case TK_STRING_LIT:
        name = read_qchar(token);
        if (name == NULL)
            goto err_invalid;
        lex = open_local_header(ctx, name);
        break;
    default:
        goto err_invalid;
    }
    free_token(token);
    dir_expect_newline(ctx);

    if (!lex)
        pp_err(ctx, "Can't locate header file: %s", name);
    free(name);
    pp_push_lex_frame(ctx, lex);
    return;

err_invalid:
    pp_err(ctx, "Invalid header name");
}

void handle_directive(PpContext *ctx)
{
    Token *token;

    token = dir_read(ctx);
    // Directives must not end before a newline
    if (!token)
        pp_err(ctx, "Expected newline at the end of empty directive");

    // Empty directive
    if (token->type == TK_NEW_LINE) {
        free_token(token);
        return;
    }
    // Otherwise the directive name must follow
    if (token->type != TK_IDENTIFIER)
        pp_err(ctx, "Pre-processing directive name must be an identifier");

    // Check for all supported directives
    if (!strcmp(token->data, "define"))
        dir_define(ctx);
    else if (!strcmp(token->data, "undef"))
        dir_undef(ctx);
    else if (!strcmp(token->data, "if"))
        dir_if(ctx, eval_if(ctx));
    else if (!strcmp(token->data, "ifdef"))
        dir_if(ctx, eval_ifdef(ctx));
    else if (!strcmp(token->data, "ifndef"))
        dir_if(ctx, !eval_ifdef(ctx));
    else if (!strcmp(token->data, "elif"))
        dir_else(ctx);
    else if (!strcmp(token->data, "else"))
        dir_else(ctx);
    else if (!strcmp(token->data, "endif"))
        dir_endif(ctx);
    else if (!strcmp(token->data, "include"))
        dir_include(ctx);
    else
        pp_err(ctx, "Unknown pre-prerocessing directive");

    // Free directive name
    free_token(token);
}
