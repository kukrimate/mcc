// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: internal definitions
//

#ifndef PP_DEF_H
#define PP_DEF_H

typedef enum {
    R_TOKEN,     // Replace with a token
    R_PARAM_EXP, // Replace with a parameter expanded
    R_PARAM_STR, // Replace with a parameter stringified
    R_PARAM_GLU, // Replace with a parameter as is
} ReplaceType;

typedef struct Replace Replace;
struct Replace {
    ReplaceType type;      // Replace type
    Token*      token;     // Original token
    _Bool       glue_next; // Entry was on the LHS of a glue operator
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
    _Bool     has_varargs;   // Does this macro have varargs parameter?
    TokenList formals;       // Formal parameters

    Macro     *next;
};

typedef enum {
    F_LEXER,   // Directly from the lexer
    F_LIST,    // List of tokens (stored in the frame)
} FrameType;

typedef struct Frame Frame;
struct Frame {
    FrameType type;
    union {
        // F_LEXER
        LexCtx *lex;    // Lexer context
        // F_LIST
        struct {
            Macro *source;  // Originating macro
            size_t i;       // Current index into the list
            TokenList list;
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

VEC_GEN(const char *, SearchDirs, dirs)

struct PpContext {
    // Parent context for pre-expansion
    PpContext *parent;
    // Header search directories
    SearchDirs search_dirs;
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

// Pre-processor stack manipulation
void pp_push_lex_frame(PpContext *ctx, LexCtx *lex);
TokenList *pp_push_list_frame(PpContext *ctx, Macro *source);
// Read the next token
Token *pp_read(PpContext *ctx);

// Macro database manipulation
Macro *new_macro(PpContext *ctx);
Macro *find_macro(PpContext *ctx, Token *token);
void free_macro(Macro *macro);
void del_macro(PpContext *ctx, Token *token);

// Conditional stack manipulation
Cond *new_cond(PpContext *ctx, CondType type);
Cond *pop_cond(PpContext *ctx);

// Evaluate a constant expression from a stored token sequence
long eval_cexpr(TokenList *list);

// Handle a pre-processor directive
void handle_directive(PpContext *ctx);

#endif
