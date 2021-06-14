// SPDX-License-Identifier: GPL-2.0-only

//
// Preprocessor
//

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <lib/vec.h>
#include "token.h"
#include "lex.h"
#include "cexpr.h"
#include "search.h"
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
            LexCtx *lex;    // Lexer context
            Token  *prev;   // For peeking
        };
        // F_LIST
        struct {
            Macro *source; // Originating macro
            Token *tokens; // Head of token list
        };
    };
    Frame *next;
};

typedef enum {
    C_IF,    // #if, #ifdef, or #ifndef
    C_ELIF,  // #elif
    C_ELSE,  // #else
    C_ENDIF, // #endif
} CondType;

typedef struct Cond Cond;
struct Cond {
    CondType type;
    Cond *next;
};

//
// Preprocessor context
//

struct PpContext {
    // Header search directories
    Vec_cstr search_dirs;
    // Enable header name mode?
    _Bool header_name;
    // Preprocessor frames
    Frame *frames;
    // Defined macros
    Macro *macros;
    // Conditional-inclusion stack
    Cond  *conds;
};

static void __attribute__((noreturn)) pp_err(PpContext *ctx, const char *err, ...)
{
    Frame *file_frame = ctx->frames;
    while (file_frame->type != F_LEXER) {
        file_frame = file_frame->next;
        assert(file_frame != NULL);
    }

    fflush(stdout);
    fprintf(stderr, "\nError in: %s:%ld: ", lex_filename(file_frame->lex),
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

static void drop_frame(PpContext *ctx)
{
    Frame *tmp;

    tmp = ctx->frames;
    if (tmp->type == F_LEXER)
        lex_free(tmp->lex);
    ctx->frames = ctx->frames->next;
    free(tmp);
}

void pp_push_lex_frame(PpContext *ctx, LexCtx *lex)
{
    Frame *frame;

    frame = new_frame(ctx);
    frame->type = F_LEXER;
    frame->lex = lex;
}

void pp_push_list_frame(PpContext *ctx, Macro *source, Token *tokens)
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
            token = lex_next(frame->lex, ctx->header_name);
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
            token = frame->prev = lex_next(frame->lex, ctx->header_name);
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

static Cond *new_cond(PpContext *ctx, CondType type)
{
    Cond *cond = calloc(1, sizeof *cond);
    cond->type = type;
    cond->next = ctx->conds;
    ctx->conds = cond;
    return cond;
}

static Cond *pop_cond(PpContext *ctx)
{
    Cond *cond = ctx->conds;
    if (cond)
        ctx->conds = cond->next;
    return cond;
}

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
            pp_err(ctx, "Non-empty argument list for 0 parameter macro");
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
            pp_err(ctx, "Unexpected end of parameters");

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
                pp_err(ctx, "Too many parameters for function-like macro");
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
        pp_err(ctx, "Too few parameters for function-like macro");
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
            pp_push_list_frame(&subctx, NULL, dup_tokens(actuals[replace->param_idx]));
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

// Pre-defined macros
typedef struct {
    // Identifier
    const char *name;
    // Handler function
    void (*handle)(PpContext *ctx);
} MccPredef;


static void handle_date(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_PP_NUMBER, strdup("Mar  7 2021")));
}


static void handle_time(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, strdup("15:30:07")));
}

static void handle_file(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, strdup("unknown")));
}

static void handle_line(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
        create_token(TK_STRING_LIT, strdup("1")));
}

static void handle_one(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
     create_token(TK_PP_NUMBER, strdup("1")));
}

static void handle_vers(PpContext *ctx)
{
    pp_push_list_frame(ctx, NULL,
     create_token(TK_PP_NUMBER, strdup("199901L")));
}

// Pre-defined macros
static MccPredef predefs[] = {
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

    // Only identifiers can expand (including pre-defined macros)
    if (identifier->type != TK_IDENTIFIER)
        return identifier;

    // Check for pre-defined macro
    for (i = 0; i < sizeof predefs / sizeof *predefs; ++i) {
        if (!strcmp(predefs[i].name, identifier->data)) {
            predefs[i].handle(ctx);
            goto retry_inherit_space;
        }
    }

    // Check if token has the no expand flag set
    if (identifier->no_expand)
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
    pp_push_list_frame(ctx, macro, expansion);

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
        pp_err(ctx, "Formal parameters must start with (");

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
        pp_err(ctx, "Unexpected end of formal parameters");
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
        pp_err(ctx, "Invalid token in formal parameter list");

    // Make sure it's not duplicate
    if (find_formal(macro, tmp) >= 0)
        pp_err(ctx, "Duplicate formal parameter name");

    // Add name to the list
    *tail = tmp;
    tail = &(*tail)->next;
    ++macro->param_cnt;

    // Next token must be a , or )
    tmp = pp_readline(ctx);
    if (!tmp)
        pp_err(ctx, "Unexpected end of formal parameters");
    if (tmp->type == TK_COMMA) {
        if (macro->has_varargs)
            pp_err(ctx, "Variable args must be the last formal parameter of macro");
        goto want_ident;
    }
    if (tmp->type != TK_RIGHT_PAREN)
        pp_err(ctx, "Invalid token in formal parameter list");
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
                pp_err(ctx, "## operator must not be the first token of a replacement list");

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
                    pp_err(ctx, "# operator must be followed by formal parameter name");
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
        pp_err(ctx, "## operator must not be the last token in a replacement list");
}

static void dir_define(PpContext *ctx)
{
    Token *tmp;
    Macro *macro;

    // Macro name must be an identifier
    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        pp_err(ctx, "Macro name must be an identifier");

    // Put macro name into database and get pointer to struct
    macro = new_macro(ctx);
    macro->name = tmp;
    macro->enabled = 1;

    // Check for macro type
    tmp = pp_peek(ctx);
    if (tmp && tmp->type == TK_LEFT_PAREN && !tmp->lwhite) {
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
        pp_err(ctx, "Macro name must be an identifier");
    // Delete macro
    del_macro(ctx, tmp);
    free_token(tmp);
}

static _Bool is_predef(Token *identifier)
{
    size_t i;

    if (!identifier || identifier->type != TK_IDENTIFIER)
        return 0;

    for (i = 0; i < sizeof predefs / sizeof *predefs; ++i) {
        if (!strcmp(predefs[i].name, identifier->data)) {
            return 1;
        }
    }
    return 0;
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
            result = is_predef(*tail) || find_macro(ctx, *tail);
            free_token(*tail);
            if (result)
                *tail = create_token(TK_PP_NUMBER, strdup("1"));
            else
                *tail = create_token(TK_PP_NUMBER, strdup("0"));

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
    subctx.frames = NULL;
    subctx.macros = ctx->macros;
    pp_push_list_frame(&subctx, NULL, head);

    head = NULL;
    tail = &head;
    while ((*tail = pp_expand(&subctx))) {
        // Replace un-replaced identifiers with 0
        if ((*tail)->type == TK_IDENTIFIER) {
            free_token(*tail);
            *tail = create_token(TK_PP_NUMBER, strdup("0"));
        }
        tail = &(*tail)->next;
    }

    // Finally evaluate the expression
    result = eval_cexpr(head);
    free_tokens(head);
    return result;

err_defined:
    pp_err(ctx, "Missing/malformed argument for defined operator");
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
        if (tmp->type == TK_HASH && tmp->directive) {
            // Read directive name
            free_token(tmp);
            tmp = pp_read(ctx);
            if (!tmp)
                goto err;

            // We don't care about empty or invalid directives here
            if (tmp->lnew || tmp->type != TK_IDENTIFIER)
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

static _Bool is_defined(PpContext *ctx)
{
    Token *tmp;
    _Bool result;

    tmp = pp_readline(ctx);
    if (!tmp || tmp->type != TK_IDENTIFIER)
        pp_err(ctx, "#if(n)def must be followed by a macro name");

    result = is_predef(tmp) || find_macro(ctx, tmp);
    free_token(tmp);
    return result;
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

// #include directive
static void dir_include(PpContext *ctx)
{
    Token  *hname;
    LexCtx *lex;

    ctx->header_name = 1;
    hname = pp_readline(ctx);
    if (!hname)
        pp_err(ctx, "Missing header name from #include");
    ctx->header_name = 0;

    char *name = hname->data + 1;
    name = strndup(name, strlen(name) - 1);

    switch (hname->type) {
    case TK_HCHAR_LIT:
        lex = open_system_header(ctx, name);
        break;
    case TK_QCHAR_LIT:
        lex = open_local_header(ctx, name);
        break;
    default:
        pp_err(ctx, "Invalid header name");
        break;
    }
    free_token(hname);

    if (!lex)
        pp_err(ctx, "Can't locate header file: %s", name);

    free(name);
    pp_push_lex_frame(ctx, lex);
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

PpContext *pp_create(void)
{
    PpContext *ctx = calloc(1, sizeof *ctx);
    vec_cstr_init(&ctx->search_dirs);
    return ctx;
}

void pp_free(PpContext *ctx)
{
    vec_cstr_free(&ctx->search_dirs);
    free_macros(ctx->macros);
    free(ctx);
}

void pp_add_search_dir(PpContext *ctx, const char *dir)
{
    vec_cstr_add(&ctx->search_dirs, dir);
}

int pp_push_file(PpContext *ctx, const char *path)
{
    LexCtx *lex = lex_open_file(path);
    if (!lex)
        return -1;

    pp_push_lex_frame(ctx, lex);
    return 0;
}

void pp_push_string(PpContext *ctx, const char *filename, const char *string)
{
    pp_push_lex_frame(ctx, lex_open_string(filename, string));
}
