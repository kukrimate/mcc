#ifndef PP_H
#define PP_H

typedef enum {
    F_LEXER,  // Directly from the lexer
    F_LIST,   // List of tokens in memory
} frame_type;

typedef struct {
    frame_type type;
    union {
        // F_LEXER
        struct {
            LexCtx ctx;
        };
        // F_LIST
        struct {
            // Token list
            VECtoken      tokens;
            // Current position in token list
            size_t        token_idx;
        };
    };
} frame;

// Vector of frames (as the preprocessor "stack")
VEC_GEN(frame, frame);

// Exit with an error
void pp_err(void);

// Push an Io handle
void frame_push_file(VECframe *frame_stack, Io *io);

// Push a list of tokens
void frame_push_list(VECframe *frame_stack, VECtoken tokens);

// Read the next frame
token frame_next(VECframe *frame_stack);

// Peek at the next frame
token frame_peek(VECframe *frame_stack);

#endif
