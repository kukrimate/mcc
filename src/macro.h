#ifndef MACRO_H
#define MACRO_H

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
            size_t index; // Index of the parameter
        };
    };
} replace;

VEC_GEN(replace, replace)

typedef struct {
    // Is this macro disabled?
    _Bool      disabled;
    // Is this a function like macro?
    _Bool      function_like;
    // Number of parameters (if function like)
    size_t     num_params;
    // Replacement list
    VECreplace replacement_list;
} macro;

// Hash map for storing macros
MAP_GEN(const char *, macro, djb2_hash, !strcmp, macro)

token next_token_expand(VECframe *frame_stack, MAPmacro *macro_database);

void dir_define(VECframe *frame_stack, MAPmacro *macro_database);

void dir_undef(VECframe *frame_stack, MAPmacro *macro_database);

#endif
