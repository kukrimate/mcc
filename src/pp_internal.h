// Required headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "token.h"
#include "io.h"
#include "lex.h"

typedef enum {
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

typedef enum {
    M_OBJECT,   // Object-like macro
    M_FUNCTION, // Function-like macro
} MacroType;

typedef struct Macro Macro;
struct Macro {
    Token     *name;   // Name of this macro
    _Bool     enabled; // Is this macro enabled?
    MacroType type;    // Type of macro
    union {
        // M_OBJECT
        struct {
            Token   *token_list;   // Replacement list
        };
        // M_FUNCTION
        struct {
            size_t   param_cnt;     // Number of parameters
            VECToken *formal_list;  // Formal parameters
            Replace  *replace_list; // Replacement list
        };
    };
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
            Macro    *source;     // Originating macro
            VECToken *token_list; // Head of token list
        };
    };
    Frame *next;
};

// Preprocessor context
typedef struct {
    Frame *frame_list;
    Macro *macro_list;
} PpContext;
