// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: internal definitions
//

#ifndef PP_DEF_H
#define PP_DEF_H

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
    // Parent context for pre-expansion
    PpContext *parent;
    // Header search directories
    Vec_cstr search_dirs;
    // Preprocessor frames
    Frame *frames;
    // Defined macros
    Macro *macros;
    // Conditional-inclusion stack
    Cond  *conds;
};

//
// Pre-defined macros
//

typedef struct {
    // Identifier
    const char *name;
    // Handler function
    void (*handle)(PpContext *ctx);
} Predef;

Predef *find_predef(Token *identifier);


// Print an error message then exit
void __attribute__((noreturn)) pp_err(PpContext *ctx, const char *err, ...);

void pp_push_lex_frame(PpContext *ctx, LexCtx *lex);
void pp_push_list_frame(PpContext *ctx, Macro *source, Token *tokens);
void drop_frame(PpContext *ctx);

// Read the next token
Token *pp_read(PpContext *ctx);
// Peek at the next token
Token *pp_peek(PpContext *ctx);
// Read the next token until a newline
Token *pp_readline(PpContext *ctx);

Macro *new_macro(PpContext *ctx);
Macro *find_macro(PpContext *ctx, Token *token);
void free_macro(Macro *macro);
void del_macro(PpContext *ctx, Token *token);

Cond *new_cond(PpContext *ctx, CondType type);
Cond *pop_cond(PpContext *ctx);

// Evaluate a constant expression from a stored token sequence
long eval_cexpr(Token *head);

// Handle a pre-processor directive
void handle_directive(PpContext *ctx);

#endif
