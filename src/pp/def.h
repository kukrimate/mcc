// SPDX-License-Identifier: GPL-2.0-only

//
// Pre-processor: internal definitions
//

#ifndef PP_DEF_H
#define PP_DEF_H

typedef enum {
    R_TOKEN,   // Token
    R_PARAM,   // Parameter
    R_OP_STR,  // Operand of #
    R_OP_GLU,  // Operand of ##
} ReplaceType;

typedef struct {
    ReplaceType type;      // Replace type
    Token       *token;    // Original token
    _Bool       glue_next; // Entry was on the LHS of a glue operator
    ssize_t     param_idx; // Parameter index (for R_PARAM_*) or -1
} Replace;

VEC_GEN(Replace, ReplaceList, replace_list)

typedef struct Macro Macro;
struct Macro {
    Token       *name;         // Name of this macro
    _Bool       enabled;       // Is this macro enabled?
    _Bool       function_like; // Is this macro function like?
    ReplaceList replace_list;  // Replacement list

    // Function-like macro-only
    _Bool       has_varargs;   // Does this macro have varargs parameter?
    TokenList   formals;       // Formal parameters

    Macro     *next;
};

typedef enum {
    C_IF,    // #if, #ifdef, or #ifndef
    C_ELIF,  // #elif
    C_ELSE,  // #else
    C_ENDIF, // #endif
} Cond;

VEC_GEN(Cond, CondList, cond_list)

typedef enum {
    F_LEXER,   // Directly from the lexer
    F_LIST,    // List of tokens (stored in the frame)
} FrameType;

typedef struct Frame Frame;
struct Frame {
    FrameType type;
    union {
        // F_LEXER
        struct {
            LexCtx      *lex;     // Lexer context
            CondList    conds;    // Conditional inclusion stack
        };
        // F_LIST
        struct {
            Macro       *source;  // Originating macro
            TokenList   list;     // List of tokens
            size_t      i;        // Current index into the list
        };
    };
    Frame *next;
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
    // Translation time and date
    struct tm *start_time;
    // Preprocessor frames
    Frame *frames;
    // Defined macros
    Macro *macros;
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

// Evaluate a constant expression
long eval_cexpr(PpContext *pp);

// Handle a pre-processor directive
void handle_directive(PpContext *ctx);

#endif
