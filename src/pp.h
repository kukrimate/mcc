#ifndef PP_H
#define PP_H

typedef enum {
    R_TOKEN,     // Replace with a token
    R_PARAM_EXP, // Replace with a parameter expanded
    R_PARAM_STR, // Replace with a parameter stringified
    R_PARAM_GLU, // Replace with a parameter without expansion (or placemarker)
} replace_type;

typedef struct {
    replace_type type;
    union {
        // R_TOKEN
        struct {
            token  token; // Token to append
        };
        // R_PARAM, R_PARAM_STR
        struct {
            _Bool  lwhite; // Inherit spacing of replacement list
            size_t index; // Index of the parameter
        };
    };
} replace;

VEC_GEN(replace, replace)

typedef struct {
    // Is this macro enabled?
    _Bool      enabled;
    // Is this a function like macro?
    _Bool      function_like;
    // Formal parameters
    VECtoken   formals;
    // Replacement list
    VECreplace replacement_list;
} macro;

typedef enum {
    F_LEXER,  // Directly from the lexer
    F_LIST,   // List of tokens in memory
} frame_type;

typedef struct {
    frame_type type;
    union {
        // F_LEXER
        struct {
            Io *  io;
            _Bool last_valid;
            token last;
        };
        // F_LIST
        struct {
            // Macro this frame was expanded from
            macro    *source;
            // Token list
            VECtoken tokens;
            // Current position in token list
            size_t   token_idx;
        };
    };
} frame;

// Vector of frames (as the preprocessor "stack")
VEC_GEN(frame, frame);

// Exit with an error
void pp_err(void);

// Push an Io handle
void frame_push_file(VECframe *frame_stack, Io *io);

// Push a list of tokens (with an optional macro as a source)
void frame_push_list(VECframe *frame_stack, macro *source, VECtoken tokens);

// Read the next frame
token frame_next(VECframe *frame_stack);

// Peek at the next frame
token frame_peek(VECframe *frame_stack);

// Hash map for storing macros
MAP_GEN(const char *, macro, djb2_hash, !strcmp, macro)

token next_token_expand(VECframe *frame_stack, MAPmacro *macro_database);

void dir_define(VECframe *frame_stack, MAPmacro *macro_database);

void dir_undef(VECframe *frame_stack, MAPmacro *macro_database);

#endif
