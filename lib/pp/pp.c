/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "io.h"
#include "lex.h"
#include "err.h"
#include "pp.h"

typedef enum {
    R_GLUE,      // Glue operator
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
    _Bool     has_varargs;   // Does this macro have varargs parameter?
    Token     *formals;      // Formal parameters

    Macro     *next;
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
            _Bool first;   // First token from this frame?
            Token *prev;   // For peeking
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
struct PpContext {
    // Preprocessor frames
    Frame *frames;
    // Defined macros
    Macro *macros;
};

static Frame *new_frame(PpContext *ctx)
{
    Frame *frame = calloc(1, sizeof *frame);
    frame->next = ctx->frames;
    ctx->frames = frame;
    return frame;
}

static void drop_frame(PpContext *ctx)
{
    Frame *tmp;

    tmp = ctx->frames;
    if (tmp->type == F_LEXER)
        io_close(tmp->io);
    ctx->frames = ctx->frames->next;
    free(tmp);
}

void pp_push_file(PpContext *ctx, Io *io)
{
    Frame *frame;

    frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->first = 1;
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

// Read the next token
static Token *pp_read(PpContext *ctx)
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
            token = lex_next(frame->io, 0);
            if (!token) {
                // Remove frame
                drop_frame(ctx);
                goto recurse;
            }
            // Mark appropriate token from the lexer as a directive
            if (frame->first || token->lnew) {
                token->directive = 1;
                frame->first = 0;
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

// Peek at the next token
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
        if (frame->prev) {
            // Return saved token
            token = frame->prev;
        } else {
            // Fill frame->prev if it doesn't exist
            token = frame->prev = lex_next(frame->io, 0);
            if (!token) {
                // Peek at next frame
                frame = frame->next;
                goto recurse;
            }
            // Mark appropriate token from the lexer as a directive
            if (frame->first || token->lnew) {
                token->directive = 1;
                frame->first = 0;
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

// Read the next token until a newline
static Token *pp_readline(PpContext *ctx)
{
    Token *token;

    token = pp_peek(ctx);
    if (token && !token->lnew)
        return pp_read(ctx);
    return NULL;
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
    Token *tmp, **tmps;
    size_t actual_cnt, paren_nest;

    // Argument list needs to start with (
    tmp = pp_read(ctx);
    if (!tmp || tmp->type != TK_LEFT_PAREN)
        mcc_err("Missing ( for function-like macro call");
    free_token(tmp);

    // Just check for closing parenthesis for 0 paramter macro
    if (!macro->param_cnt) {
        tmp = pp_read(ctx);
        if (!tmp || tmp->type != TK_RIGHT_PAREN)
            mcc_err("Non-empty argument list for 0 parameter macro");
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
            mcc_err("Unexpected end of parameters");

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
                mcc_err("Too many parameters for function-like macro");
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

    // Free right paren
    free_token(tmp);

    // Make sure we got the correct number of actuals
    if (macro->param_cnt != actual_cnt)
        mcc_err("Too few parameters for function-like macro");
}


static Token *glue_free(Token *l, Token *r)
{
    Token *result;

    result = glue(l, r);
    free_token(l);
    free_token(r);
    return result;
}

Token *pp_expand(PpContext *ctx);

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
            subctx.frames = NULL;
            subctx.macros = ctx->macros;
            pp_push_list(&subctx, NULL, dup_tokens(actuals[replace->param_idx]));
            for (first = 1;; first = 0) {
                tmp = pp_expand(&subctx);
                if (!tmp)
                    break;
                // Inherit spacing from replacement list
                if (first)
                    tmp->lwhite = replace->token->lwhite;
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
                tmp = create_token(TK_PLACEMARKER, NULL);
                glue_tmp
            }
            break;
        }

    #undef glue_tmp
    return head;
}

static void handle_directive(PpContext *ctx);

Token *pp_expand(PpContext *ctx)
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
        goto retry_inherit_space;

    // Check for pre-processing directive
    if (identifier->type == TK_HASH && identifier->directive) {
        handle_directive(ctx);
        goto retry_free;
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
    pp_push_list(ctx, macro, expansion);

retry_inherit_space:
    // Next token on the stream inherits our spacing
    if ((last = pp_peek(ctx))) {
        last->lnew = identifier->lnew;
        last->lwhite = identifier->lwhite;
    }
retry_free:
    // Free identifier
    free_token(identifier);
    // Continue recursively expanding
    goto recurse;
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

static void capture_formals(PpContext *ctx, Macro *macro)
{
    Token **tail, *tmp;

    // First token must be an lparen
    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_LEFT_PAREN)
        mcc_err("Formal parameters must start with (");

    // Initialize token list
    tail = &macro->formals;
    macro->param_cnt = 0;
    macro->has_varargs = 0;

want_ident:
    // Free previous token
    free_token(tmp);

    // Read formal parameter name
    tmp = pp_readline(ctx);
    if (!tmp)
        mcc_err("Unexpected end of formal parameters");
    if (tmp->type == TK_RIGHT_PAREN)
        goto end;
    // Variable argument mode
    if (tmp->type == TK_VARARGS) {
        free_token(tmp);
        tmp = create_token(TK_IDENTIFIER, strdup("__VA_ARGS__"));
        macro->has_varargs = 1;
    }
    // Must be an identifier
    if (tmp->type != TK_IDENTIFIER)
        mcc_err("Invalid token in formal parameter list");

    // Make sure it's not duplicate
    if (find_formal(macro, tmp) >= 0)
        mcc_err("Duplicate formal parameter name");

    // Add name to the list
    *tail = tmp;
    tail = &(*tail)->next;
    ++macro->param_cnt;

    // Next token must be a , or )
    tmp = pp_readline(ctx);
    if (!tmp)
        mcc_err("Unexpected end of formal parameters");
    if (tmp->type == TK_COMMA) {
        if (macro->has_varargs)
            mcc_err("Variable args must be the last formal parameter of macro");
        goto want_ident;
    }
    if (tmp->type != TK_RIGHT_PAREN)
        mcc_err("Invalid token in formal parameter list");
end:
    free_token(tmp);
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

    while ((tmp = pp_readline(ctx))) {
        switch (tmp->type) {
        case TK_HASH_HASH:
            // Must be preceeded by something
            if (!macro->replace_list)
                mcc_err("## operator must not be the first token of a replacement list");

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
                tmp = pp_readline(ctx);
                if (!tmp || (formal_idx = find_formal(macro, tmp)) < 0) {
                    mcc_err("# operator must be followed by formal parameter name");
                }

                prev = *tail = calloc(1, sizeof **tail);
                (*tail)->type = R_PARAM_STR;
                (*tail)->token = tmp;
                (*tail)->param_idx = formal_idx;
                tail = &(*tail)->next;
                do_glue = 0;
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
        mcc_err("## operator must not be the last token in a replacement list");
}

static void dir_define(PpContext *ctx)
{
    Token *tmp;
    Macro *macro;

    // Macro name must be an identifier
    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        mcc_err("Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    macro = new_macro(ctx);
    macro->name = tmp;
    macro->enabled = 1;

    // Check for macro type
    tmp = pp_peek(ctx);
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
    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        mcc_err("Macro name must be an identifier");
    // Delete macro
    del_macro(ctx, tmp);
    free_token(tmp);
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
        mcc_err("Pre-processing directive must be an identifier");

    // Check for all supported directives
    if (!strcmp(tmp->data, "define"))
        dir_define(ctx);
    else if (!strcmp(tmp->data, "undef"))
        dir_undef(ctx);
    else
        mcc_err("Unknown pre-prerocessing directive");

    // Free directive name
    free_token(tmp);
}

PpContext *pp_create(const char *path)
{
    Io *io;
    PpContext *ctx;

    // Open file
    io = io_open(path);
    if (!io)
        return NULL;

    // Create context
    ctx = calloc(1, sizeof *ctx);
    pp_push_file(ctx, io);

    return ctx;
}

void pp_free(PpContext *ctx)
{
    free_macros(ctx->macros);
    free(ctx);
}
