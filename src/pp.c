/*
 * Preprocessor
 */

#include "pp_internal.h"
#include "pp.h"

void pp_err(char *error)
{
    fprintf(stderr, "Preprocessor error: %s\n", error);
    exit(1);
}

static Frame *new_frame(PpContext *ctx)
{
    Frame *frame = calloc(1, sizeof *frame);
    frame->next = ctx->frame_list;
    ctx->frame_list = frame;
    return frame;
}

void pp_push_file(PpContext *ctx, Io *io)
{
    Frame *frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->io = io;
}

void pp_push_list(PpContext *ctx, Macro *source, VECToken *token_list)
{
    // Disable source macro if present
    if (source)
        source->enabled = 0;

    Frame *frame = new_frame(ctx);
    frame->type = F_LIST;
    frame->source = source;
    frame->token_list = token_list;
}

Token *pp_next(PpContext *ctx)
{
    Frame *frame;
    Token *token;

recurse:
    frame = ctx->frame_list;
    if (!frame)
        return NULL;
    switch (frame->type) {
    case F_LEXER:
        // Get saved token
        token = frame->last;
        frame->last = NULL;
        if (!token) {
            // Read token directly from lexer
            token = lex_next(frame->io);
            if (!token) {
                // Remove frame
                ctx->frame_list = frame->next;
                goto recurse;
            }
        }
        break;
    case F_LIST:
        // Get token from the token list
        if (!frame->token_list) {
            // Re-enable source macro when popping frame
            if (frame->source)
                frame->source->enabled = 1;
            // Remove frame
            ctx->frame_list = frame->next;
            goto recurse;
        }
        // Advance token list
        token = frame->token_list;
        frame->token_list = frame->token_list->next;
        break;
    }

    return token;
}

Token *pp_peek(PpContext *ctx)
{
    Frame *frame;
    Token *token;

    frame = ctx->frame_list;
recurse:
    if (!frame)
        return NULL;
    switch (frame->type) {
    case F_LEXER:
        // Fill frame->last if it doesn't exist
        if (!frame->last) {
            frame->last = lex_next(frame->io);
            if (!frame->last) {
                // Peek at next frame
                frame = frame->next;
                goto recurse;
            }
        }
        // Return frame->last
        token = frame->last;
        break;
    case F_LIST:
        token = frame->token_list;
        if (!token) {
            // Peek at next frame
            frame = frame->next;
            goto recurse;
        }
        break;
    }

    return token;
}

static Macro *new_macro(PpContext *ctx)
{
    Macro *macro = calloc(1, sizeof *macro);
    macro->next = ctx->macro_list;
    ctx->macro_list = macro;
    return macro;
}

static Macro *find_macro(PpContext *ctx, Token *token)
{
    Macro *macro;

    for (macro = ctx->macro_list; macro; macro = macro->next) {
        if (!strcmp(macro->name->data, token->data)) {
            return macro;
        }
    }
    return NULL;
}

static void del_macro(PpContext *ctx, Token *token)
{
    Macro **macro;

    for (macro = &ctx->macro_list; *macro; macro = &(*macro)->next) {
        // Delete macro if name matches, then return
        if (!strcmp((*macro)->name->data, token->data)) {
            *macro = (*macro)->next;
            return;
        }
    }
}

static void capture_actuals(PpContext *ctx, Macro *macro, Token **actuals)
{
    Token *token, **tokens;
    size_t actual_cnt, paren_nest;

    // Argument list needs to start with (
    token = pp_next(ctx);
    if (!token || token->type != TK_LEFT_PAREN)
        pp_err("Missing ( for function-like macro call");

    // Just check for closing parenthesis for 0 paramter macro
    if (!macro->param_cnt) {
        token = pp_next(ctx);
        if (!token || token->type != TK_RIGHT_PAREN)
            pp_err("Non-empty argument list for 0 parameter macro");
        return;
    }

    // Start of actuals
    actual_cnt = 0;
    tokens = &actuals[actual_cnt++];

    // Start at 1 deep parenthesis
    paren_nest = 1;

    for (;;) {
        token = pp_next(ctx);
        if (!token)
            pp_err("Unexpected end of parameters");

        switch (token->type) {
        case TK_COMMA:
            // Ignore comma in nested parenthesis
            if (paren_nest > 1)
                goto add_tok;
            // Move to next actual parameter
            if (actual_cnt >= macro->param_cnt)
                pp_err("Too many parameters for function-like macro");
            tokens = &actuals[actual_cnt++];
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
            // Add token to actual paramter's token list
            *tokens = token;
            tokens = &(*tokens)->next;
            break;
        }
    }
    endloop:

    // Make sure we got the correct number of actuals
    if (macro->param_cnt != actual_cnt)
        pp_err("Too few parameters for function-like macro");
}

static Token *pp_next_expand(PpContext *ctx);

static Token *expand_macro(PpContext *ctx, Macro *macro, Token **actuals)
{
    Token     *expansion, **expansion_tail;
    Replace   *replace;
    Token     *token;
    PpContext subctx;
    _Bool     first;

    expansion = NULL;
    expansion_tail = &expansion;
    for (replace = macro->replace_list; replace; replace = replace->next) {
        // Token from the replacement list becomes part of the result
        if (replace->type == R_TOKEN) {
            *expansion_tail = dup_token(replace->token);
            expansion_tail = &(*expansion_tail)->next;
            continue;
        }

        // Now we will need to deal with the actuals in some ways
        switch (replace->type) {
        case R_PARAM_EXP:
            // We need to pre-expand the actual paramter
            subctx.frame_list = NULL;
            subctx.macro_list = ctx->macro_list;
            pp_push_list(&subctx, NULL, actuals[replace->param_idx]);
            // Macro expand the actuals
            for (first = 1;; first = 0) {
                // Read token from subcontext
                token = pp_next_expand(&subctx);
                if (!token)
                    break;
                // Inherit spacing from replacement list
                if (first) {
                    token->lwhite = replace->token->lwhite;
                    token->lnew = replace->token->lnew;
                }
                // Append to expansion
                *expansion_tail = token;
                expansion_tail = &(*expansion_tail)->next;
            }
            break;
        case R_PARAM_STR:
            token = stringize(actuals[replace->param_idx]);
            *expansion_tail = token;
            expansion_tail = &(*expansion_tail)->next;
            break;
        case R_PARAM_GLU:
            // Add placemarker if actual is empty
            token = actuals[replace->param_idx];
            if (!token)
                token = create_token(TK_PLACEMARKER, NULL);
            for (; token; token = token->next) {
                *expansion_tail = dup_token(token);
                expansion_tail = &(*expansion_tail)->next;
            }
            break;
        default:
            break;
        }
    }

    return expansion;
}

Token *pp_next_expand(PpContext *ctx)
{
    Token  *token, *lparen, *expansion;
    Macro  *macro;
    Token  **actuals;

recurse:
    // Read token from the frame stack
    token = pp_next(ctx);
    if (!token)
        return NULL;

    // Ignore placemarkers resulting from previous expansions
    if (token->type == TK_PLACEMARKER)
        goto recurse;

    // Return token if no macro expansion is required
    if (token->type != TK_IDENTIFIER || token->no_expand)
        return token;

    // See if the token is a macro name
    macro = find_macro(ctx, token);
    if (!macro)
        return token;

    // If macro is disabled here, this token can *never* expand again
    if (!macro->enabled) {
        token->no_expand = 1;
        return token;
    }

    // printf("Expanding: %s\n", token->data);

    // We need to capture the catuals if the macro is function like
    switch (macro->type) {
    case M_FUNCTION:
        // If there are no actuals provided, we don't expand
        lparen = pp_peek(ctx);
        if (!lparen || lparen->type != TK_LEFT_PAREN)
            return token;
        // Capture the actuals
        actuals = calloc(macro->param_cnt, sizeof *actuals);
        capture_actuals(ctx, macro, actuals);
        // Expand macro
        expansion = expand_macro(ctx, macro, actuals);
        // Free actuals
        free(actuals);
        break;
    case M_OBJECT:
        // Expand macro
        expansion = macro->token_list;
        break;
    }

    // First resulting token inherits the macro identifier's spacing
    if (expansion) {
        expansion->lwhite = token->lwhite;
        expansion->lnew = token->lnew;
    }

    // Push result after a successful expansion
    pp_push_list(ctx, macro, expansion);
    // Continue recursively expanding
    goto recurse;
}

static void capture_formal_list(PpContext *ctx, Macro *macro)
{
    Token **tail;
    Token *token;

    macro->param_cnt = 0;
    tail = &macro->formal_list;
    for (;;) {
        token = pp_next(ctx);
        if (!token)
            pp_err("Unexpected end of formal parameters");

        switch (token->type) {
        case TK_IDENTIFIER:
            // Save formal parameter
            *tail = token;
            tail = &(*tail)->next;
            ++macro->param_cnt;
            // Paramter name must be followed either by a , or )
            token = pp_next(ctx);
            if (token->type == TK_RIGHT_PAREN) // End of formals
                return;
            else if (token->type != TK_COMMA)
                pp_err("Invalid token in formal parameter list");
            break;
        case TK_RIGHT_PAREN: // End of formals
            return;
        default:             // No other tokens are valid here
            pp_err("Invalid token in formal parameter list");
        }
    }
}

static ssize_t find_formal(Macro *macro, Token *token)
{
    size_t idx;
    Token *formal;

    idx = 0;
    for (formal = macro->formal_list; formal; formal = formal->next) {
        if (!strcmp(formal->data, token->data))
            return idx;
        ++idx;
    }
    return -1;
}

static void capture_replace_list(PpContext *ctx, Macro *macro)
{
    Replace **tail;
    Token   *token;
    ssize_t formal_idx;
    _Bool   stringify;
    _Bool   glue;

    tail = &macro->replace_list;
    stringify = 0;
    glue = 0;
    for (;;) {
        // Newline ends replacement list
        token = pp_peek(ctx);
        if (token && token->lnew)
            goto break_loop;

        token = pp_next(ctx);
        // EOF ends replacement list
        if (!token)
            goto break_loop;

        switch (token->type) {
        case TK_HASH:
            // Wait for the right-hand-side of stringify
            stringify = 1;
            break;
        // case TK_HASH_HASH:
        //     // Must be preceeded by something
        //     if (!macro->replace_list)
        //         pp_err();

        //     // Do *not* expand parameter before ## operator
        //     if ((*tail)->type == R_PARAM_EXP)
        //         (*tail)->type = R_PARAM_GLU;

        //     // Wait for the right-hand-side of glue
        //     glue = 1;

        //     // Still need to add the operator as a token
        //     *tail = calloc(1, sizeof **tail);
        //     (*tail)->type = R_TOKEN;
        //     (*tail)->token = token;
        //     tail = &(*tail)->next;
        //     break;
        default:
            // Append token or parameter index to replacement list
            *tail = calloc(1, sizeof **tail);
            if (token->type == TK_IDENTIFIER
                    && (formal_idx = find_formal(macro, token)) >= 0) {
                (*tail)->type  = stringify ? R_PARAM_STR :
                                 (glue ? R_PARAM_GLU : R_PARAM_EXP);
                (*tail)->param_idx = formal_idx;
                (*tail)->token = token;
            } else {
                (*tail)->type = R_TOKEN;
                (*tail)->token = token;
            }
            tail = &(*tail)->next;
            stringify = 0;
            glue = 0;
            break;
        }
    }
    break_loop:

    // These must *not* be left incomplete
    if (stringify)
        pp_err("The # operator must not be last in a replacement list");
    if (glue)
        pp_err("The ## operator must not be last in a replacement list");
}

static void capture_token_list(PpContext *ctx, Macro *macro)
{
    Token **tail, *token;

    // First token will already be there
    tail = &macro->token_list->next;
    for (;;) {
        // Newline ends token list
        token = pp_peek(ctx);
        if (token && token->lnew)
            break;
        // EOF ends token list
        token = pp_next(ctx);
        if (!token)
            break;
        // Add token
        *tail = token;
        tail = &(*tail)->next;
    }
}

void dir_define(PpContext *ctx)
{
    Token *token;
    Macro *macro;

    // Macro name must be an identifier
    token = pp_next(ctx);
    if (token->type != TK_IDENTIFIER)
        pp_err("Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    macro = new_macro(ctx);
    macro->name = token;
    macro->enabled = 1;

    // Check for macro type
    token = pp_next(ctx);
    if (token->type == TK_LEFT_PAREN) {
        // Function like macro
        macro->type = M_FUNCTION;
        capture_formal_list(ctx, macro);
        capture_replace_list(ctx, macro);
    } else {
        // Object-like macro
        macro->type = M_OBJECT;
        macro->token_list = token;
        capture_token_list(ctx, macro);
    }
}

void dir_undef(PpContext *ctx)
{
    Token *token;

    // Macro name must be an identifier
    token = pp_next(ctx);
    if (token->type != TK_IDENTIFIER)
        pp_err("Macro name must be an identifier");

    // Delete macro
    del_macro(ctx, token);
}

void preprocess(Io *io)
{
    PpContext ctx = { .frame_list = NULL, .macro_list = NULL };
    _Bool     first;
    Token     *token;

    pp_push_file(&ctx, io);

    for (first = 1;; first = 0) {
        token = pp_next_expand(&ctx);
        if (!token)
            break;

        if (token->type == TK_HASH) {
            // Check if we need to recognize a directive
            if (first || token->lnew) {
                // Directive name must be an identifier
                token = pp_next(&ctx);
                if (token->type != TK_IDENTIFIER)
                    pp_err("Pre-processing directive must be an identifier");
                // Check for all supported directives
                if (!strcmp(token->data, "define"))
                    dir_define(&ctx);
                else if (!strcmp(token->data, "undef"))
                    dir_undef(&ctx);
                else
                    pp_err("Unknown pre-prerocessing directive");

                // Don't output anything after processing directive
                continue;
            }
        }

        // We need to output the token here
        output_token(token);
    }
}
