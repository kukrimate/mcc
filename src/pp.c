/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "token.h"
#include "io.h"
#include "lex.h"
#include "err.h"

// NOTE: Not sure if libkm will be kept long term
VEC_GEN(Token*, _Token)

typedef enum {
    R_GLUE,      // Glue opeartor
    R_TOKEN,     // Replace with a token
    R_PARAM_EXP, // Replace with a parameter expanded
    R_PARAM_STR, // Replace with a parameter stringified
    R_PARAM_GLU, // Replace with a parameter without expansion (or placemarker)
} ReplaceType;

typedef struct Replace Replace;
struct Replace {
    ReplaceType type;      // Replace type
    Token*      token;     // Original token
    ssize_t     param_idx; // Parameter index (for R_PARAM_*) or -1
    Replace     *next;
};

typedef struct Macro Macro;
struct Macro {
    Token     *name;         // Name of this macro
    _Bool     enabled;       // Is this macro enabled?
    _Bool     function_like; // Is this macro function like?
    Replace   *replace_list; // Replacement list

    // Function-like macro-only
    size_t    param_cnt;     // Number of parameters
    Token     *formals;      // Formal parameters

    Macro *next;
};

typedef enum {
    F_LEXER,  // Directly from the lexer
    F_LIST,   // List of tokens in memory
} FrameType;

typedef struct Frame Frame;
struct Frame {
    FrameType type;
    union {
        // F_LEXER
        struct {
            Io    *io;     // Token source
            Token *last;   // Saved token from peaking
        };
        // F_LIST
        struct {
            Macro *source; // Originating macro
            Token *tokens; // Head of token list
        };
    };
    Frame *next;
};

// Preprocessor context
typedef struct {
    Frame *frames;
    Macro *macros;
} PpContext;

static Frame *new_frame(PpContext *ctx)
{
    Frame *frame = calloc(1, sizeof *frame);
    frame->next = ctx->frames;
    ctx->frames = frame;
    return frame;
}

static void drop_frame(PpContext *ctx)
{
    Frame *cur;

    cur = ctx->frames;
    ctx->frames = cur->next;
    free(cur);
}

void pp_push_file(PpContext *ctx, Io *io)
{
    Frame *frame;

    frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->io = io;
}

void pp_push_list(PpContext *ctx, Macro *source, Token *tokens)
{
    Frame *frame;

    // Disable source macro if present
    if (source)
        source->enabled = 0;

    frame = new_frame(ctx);
    frame->type = F_LIST;
    frame->source = source;
    frame->tokens = tokens;
}

static Token *pp_next(PpContext *ctx)
{
    Frame *frame;
    Token *token;

recurse:
    frame = ctx->frames;
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

static Token *pp_peek(PpContext *ctx)
{
    Frame *frame;
    Token *token;

    frame = ctx->frames;
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

// Peek at next token till a newline
static Token *pp_peek_dir(PpContext *ctx)
{
    Token *tmp;

    tmp = pp_peek(ctx);
    if (!tmp || tmp->lnew)
        return NULL;
    return tmp;
}

// Read next token till a newline
static Token *pp_next_dir(PpContext *ctx)
{
    Token *tmp;

    tmp = pp_peek(ctx);
    if (!tmp || tmp->lnew)
        return NULL;
    return pp_next(ctx);
}

static Macro *new_macro(PpContext *ctx)
{
    Macro *macro = calloc(1, sizeof *macro);
    macro->next = ctx->macros;
    ctx->macros = macro;
    return macro;
}

static Macro *find_macro(PpContext *ctx, Token *token)
{
    Macro *macro;

    for (macro = ctx->macros; macro; macro = macro->next) {
        if (!strcmp(macro->name->data, token->data)) {
            return macro;
        }
    }
    return NULL;
}

static void free_macro(Macro *macro)
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

static void free_macros(Macro *head)
{
    Macro *tmp;

    while (head) {
        tmp = head->next;
        free_macro(head);
        head = tmp;
    }
}

static void del_macro(PpContext *ctx, Token *token)
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

static void capture_actuals(PpContext *ctx, Macro *macro, Token **actuals)
{
    Token *token, **tokens;
    size_t actual_cnt, paren_nest;

    // Argument list needs to start with (
    token = pp_next(ctx);
    if (!token || token->type != TK_LEFT_PAREN)
        pp_err("Missing ( for function-like macro call");
    free_token(token);

    // Just check for closing parenthesis for 0 paramter macro
    if (!macro->param_cnt) {
        token = pp_next(ctx);
        if (!token || token->type != TK_RIGHT_PAREN)
            pp_err("Non-empty argument list for 0 parameter macro");
        free_token(token);
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
            // Free comma
            free_token(token);
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
    // Free right paren
    free_token(token);

    // Make sure we got the correct number of actuals
    if (macro->param_cnt != actual_cnt)
        pp_err("Too few parameters for function-like macro");
}

static Token *dup_tokens(Token *head)
{
    Token *result, **tail;

    result = NULL;
    tail = &result;

    for (; head; head = head->next) {
        *tail = dup_token(head);
        tail = &(*tail)->next;
    }

    return result;
}

static Token *pp_next_expand(PpContext *ctx);

static void handle_replace(PpContext *ctx, Replace *replace, Token **actuals, VEC_Token *out)
{
    PpContext subctx;
    Token     *tmp;
    _Bool     first;

    switch (replace->type) {
    case R_GLUE:
        // This function can't possibly get GLUE
        // abort();
    case R_TOKEN:
        VEC_Token_add(out, dup_token(replace->token));
        break;
    case R_PARAM_EXP:
        // We need to pre-expand the actual parameter
        subctx.frames = NULL;
        subctx.macros = ctx->macros;
        pp_push_list(&subctx, NULL, dup_tokens(actuals[replace->param_idx]));
        for (first = 1;; first = 0) {
            // Read token from subcontext
            tmp = pp_next_expand(&subctx);
            if (!tmp)
                break;
            // Inherit spacing from replacement list
            if (first) {
                tmp->lwhite = replace->token->lwhite;
                tmp->lnew = replace->token->lnew;
            }
            // Add token
            VEC_Token_add(out, tmp);
        }
        break;
    case R_PARAM_STR:
        VEC_Token_add(out, stringize(actuals[replace->param_idx]));
        break;
    case R_PARAM_GLU:
        if (actuals[replace->param_idx]) {
            // Add all tokens from the actual parameter
            for (tmp = actuals[replace->param_idx]; tmp; tmp = tmp->next)
                VEC_Token_add(out, dup_token(tmp));
        } else {
            // Add placemarker if actual parameter is empty
            VEC_Token_add(out, create_token(TK_PLACEMARKER, NULL));
        }
        break;
    }
}

static Token *expand_macro(PpContext *ctx, Macro *macro, Token **actuals)
{
    VEC_Token lhs, rhs;
    Replace   *replace;
    _Bool     had_glue;

    // Create lists
    VEC_Token_init(&lhs);
    VEC_Token_init(&rhs);

    // Get replacement list
    replace = macro->replace_list;

    while (replace) {
        // Eat up all glue operators
        for (had_glue = 0;; had_glue = 1) {
            if (!replace || replace->type != R_GLUE)
                break;
            replace = replace->next;
        }

        if (had_glue) {
            // We need to do a glue
            Token *l, *r;

            // Get RHS token sequence
            handle_replace(ctx, replace, actuals, &rhs);
            replace = replace->next;

            // Glue first token of RHS to last token of LHS
            l = VEC_Token_pop(&lhs);
            r = rhs.arr[0];
            VEC_Token_add(&lhs, glue(l, r));
            free_token(l);
            free_token(r);

            // Add rest of RHS to LHS
            VEC_Token_addall(&lhs, rhs.arr + 1, rhs.n - 1);
            rhs.n = 0;
        } else {
            // Otherwise just process the replace element
            handle_replace(ctx, replace, actuals, &lhs);
            replace = replace->next;
        }
    }

    // Create linked-list from LHS
    {
        Token  *head, **tail;
        size_t i;

        head = NULL;
        tail = &head;

        for (i = 0; i < lhs.n; ++i) {
            *tail = lhs.arr[i];
            tail = &(*tail)->next;
        }

        VEC_Token_free(&lhs);
        VEC_Token_free(&rhs);
        return head;
    }
}

Token *pp_next_expand(PpContext *ctx)
{
    Token  *identifier, *lparen, **actuals, *expansion;
    Macro  *macro;
    size_t i;

recurse:
    // Read identifier from the frame stack
    identifier = pp_next(ctx);
    if (!identifier)
        return NULL;

    // Ignore placemarkers resulting from previous expansions
    if (identifier->type == TK_PLACEMARKER) {
        free_token(identifier);
        goto recurse;
    }

    // Return identifier if no macro expansion is required
    if (identifier->type != TK_IDENTIFIER || identifier->no_expand)
        return identifier;

    // See if the identifier is a macro name
    macro = find_macro(ctx, identifier);
    if (!macro)
        return identifier;

    // If macro is disabled here, this identifier can *never* expand again
    if (!macro->enabled) {
        identifier->no_expand = 1;
        return identifier;
    }

    // We need to capture the actuals if the macro is function like
    if (macro->function_like) {
        // If there are no actuals provided, we don't expand
        lparen = pp_peek(ctx);
        if (!lparen || lparen->type != TK_LEFT_PAREN)
            return identifier;
        // Capture the actuals
        actuals = calloc(macro->param_cnt, sizeof *actuals);
        capture_actuals(ctx, macro, actuals);
        // Expand macro
        expansion = expand_macro(ctx, macro, actuals);
        // Free actuals
        for (i = 0; i < macro->param_cnt; ++i)
            free_tokens(actuals[i]);
        free(actuals);
    } else {
        // Expand macro
        expansion = expand_macro(ctx, macro, NULL);
    }

    // First resulting tokenr inherits the macro identifier's spacing
    if (expansion) {
        expansion->lwhite = identifier->lwhite;
        expansion->lnew = identifier->lnew;
    }

    // Free identifier
    free_token(identifier);

    // Push result after a successful expansion
    pp_push_list(ctx, macro, expansion);
    // Continue recursively expanding
    goto recurse;
}

static void capture_formals(PpContext *ctx, Macro *macro)
{
    Token **tail, *tmp;

    // First token must be an lparen
    tmp = pp_next_dir(ctx);
    if (!tmp || tmp->type != TK_LEFT_PAREN)
        pp_err("Formal parameters must start with (");

    // Initialize token list
    tail = &macro->formals;
    macro->param_cnt = 0;

want_ident:
    // Free previous token
    free_token(tmp);

    // Read formal parameter name
    tmp = pp_next_dir(ctx);
    if (!tmp)
        pp_err("Formal parameter name missing");
    if (tmp->type == TK_RIGHT_PAREN)
        goto end;
    if (tmp->type != TK_IDENTIFIER)
        pp_err("Invalid token in formal parameter list");;

    // Add name to the list
    *tail = tmp;
    tail = &(*tail)->next;
    ++macro->param_cnt;

    // Next token must be a , or )
    tmp = pp_next_dir(ctx);
    if (!tmp)
        pp_err("Unexpected end of formal parameters");
    if (tmp->type == TK_COMMA)
        goto want_ident;
    if (tmp->type != TK_RIGHT_PAREN)
        pp_err("Invalid token in formal parameter list");
end:
    free_token(tmp);
}

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

static void capture_replace_list(PpContext *ctx, Macro *macro)
{
    Replace *prev, **tail;
    Token   *tmp;
    ssize_t formal_idx;
    _Bool   do_glue;

    prev = NULL;
    tail = &macro->replace_list;
    do_glue = 0;

    while ((tmp = pp_next_dir(ctx))) {
        switch (tmp->type) {
        case TK_HASH_HASH:
            // Must be preceeded by something
            if (!macro->replace_list)
                pp_err("## operator must not be the first token of a replacement list");

            // Do *not* expand parameter before ## operator
            if (prev->type == R_PARAM_EXP)
                prev->type = R_PARAM_GLU;

            // Wait for the right-hand-side of glue
            do_glue = 1;

            // Add a glue operator to the list
            prev = *tail = calloc(1, sizeof **tail);
            (*tail)->type = R_GLUE;
            (*tail)->token = tmp;
            tail = &(*tail)->next;
            break;
        case TK_HASH:
            if (macro->function_like) {
                // Free hash
                free_token(tmp);
                // Get formal parameter name
                tmp = pp_next_dir(ctx);
                if (!tmp || (formal_idx = find_formal(macro, tmp)) < 0) {
                    pp_err("# opeartor must be followed by formal parameter name");
                }

                prev = *tail = calloc(1, sizeof **tail);
                (*tail)->type = R_PARAM_STR;
                (*tail)->token = tmp;
                (*tail)->param_idx = formal_idx;
                break;
            }
            // FALLTHROUGH
        default:
            // Append token or parameter index to replacement list
            prev = *tail = calloc(1, sizeof **tail);
            if (!macro->function_like || (formal_idx = find_formal(macro, tmp)) < 0) {
                (*tail)->type = R_TOKEN;
                (*tail)->token = tmp;
            } else {
                (*tail)->type  = do_glue ? R_PARAM_GLU : R_PARAM_EXP;
                (*tail)->token = tmp;
                (*tail)->param_idx = formal_idx;
            }
            tail = &(*tail)->next;
            do_glue = 0;
            break;
        }
    }

    if (do_glue)
        pp_err("## operator must not be the last token in a replacement list");
}

static void dir_define(PpContext *ctx)
{
    Token *tmp;
    Macro *macro;

    // Macro name must be an identifier
    tmp = pp_next_dir(ctx);
    if (tmp->type != TK_IDENTIFIER)
        pp_err("Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    macro = new_macro(ctx);
    macro->name = tmp;
    macro->enabled = 1;

    // Check for macro type
    tmp = pp_peek_dir(ctx);
    if (tmp && tmp->type == TK_LEFT_PAREN) {
        // Function like macro
        macro->function_like = 1;
        capture_formals(ctx, macro);
    } else {
        // Object-like macro
        macro->function_like = 0;
    }

    // Capture replacement list
    capture_replace_list(ctx, macro);
}

static void dir_undef(PpContext *ctx)
{
    Token *tmp;

    // Macro name must be an identifier
    tmp = pp_next_dir(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        pp_err("Macro name must be an identifier");
    // Delete macro
    del_macro(ctx, tmp);
    free_token(tmp);
}

void preprocess(Io *io)
{
    PpContext ctx = { .frames = NULL, .macros = NULL };
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
                // Free hash
                free_token(token);

                // Directive name must be an identifier
                token = pp_next_dir(&ctx);
                if (!token || token->type != TK_IDENTIFIER)
                    pp_err("Pre-processing directive must be an identifier");
                // Check for all supported directives
                if (!strcmp(token->data, "define"))
                    dir_define(&ctx);
                else if (!strcmp(token->data, "undef"))
                    dir_undef(&ctx);
                else
                    pp_err("Unknown pre-prerocessing directive");

                // Free directive name
                free_token(token);
                // Don't output anything after processing directive
                continue;
            }
        }

        // We need to output the token here
        output_token(token);
        free_token(token);
    }

    free_macros(ctx.macros);
}
