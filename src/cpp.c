/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <map.h>
#include <djb2.h>
#include "lex.h"

//// TYPES start

// Vector of tokens
VEC_GEN(token, token_)

// Vector of token lists
VEC_GEN(token_vec, token_vec_)

// String to int map (for parameter indexes)
MAP_GEN(const char *, size_t, djb2_hash, !strcmp, index_)

typedef enum {
    R_TOKEN, // Replace with a token
    R_PARAM, // Replace with a parameter
} replace_type;

typedef struct {
    replace_type type;
    union {
        token  token; // Token to append
        size_t index; // Index of the parameter
    };
} replace;

// Vector for creating the replacement list
VEC_GEN(replace, replace_)

typedef struct {
    // Is this macro currently callable?
    _Bool       painted_blue;
    // Is this a function like macro?
    _Bool       function_like;
    // Number of parameters (if function like)
    size_t      num_params;
    // Replacement list
    replace_vec replacement_list;
} macro;

// Hash map for storing macros
MAP_GEN(const char *, macro, djb2_hash, !strcmp, macro_)

typedef enum {
    F_FILE,  // Directly from the lexer
    F_MACRO, // Replacement list of a macro
    F_LIST,  // List of tokens in memory
} frame_type;

typedef struct {
    frame_type type;
    union {
        // F_FILE
        struct {
            // File handle
            FILE          *fp;
        };
        // F_MACRO
        struct {
            // Macro definition
            macro         *macro;
            // Current position in replacement list
            size_t        replace_idx;
            // Arguments provided on macro call
            token_vec_vec arguments;
        };
        // F_LIST
        struct {
            // Token list
            token_vec     tokens;
            // Current position in token list
            size_t        token_idx;
        };
    };
} frame;

// Vector of frames (as the preprocessor "stack")
VEC_GEN(frame, frame_);

//// TYPES end

//// CODE start

static void
cpp_err(void)
{
    fprintf(stderr, "Preprocessor error!\n");
    exit(1);
}


static void
output_token(token *token)
{
    switch (token->type) {
    case TK_END_LINE:
        putchar('\n');
        break;
    case TK_IDENTIFIER:
    case TK_PP_NUMBER:
        printf("%s", token->data);
        break;
    case TK_CHAR_CONST:
        printf("\'%s\'", token->data);
        break;
    case TK_STRING_LIT:
        printf("\"%s\"", token->data);
        break;
    default:
        printf("%s", punctuator_str[token->type]);
    }
}

static void
frame_push_file(frame_vec *frame_stack, FILE *fp)
{
    frame *frame;

    frame = frame_vec_push(frame_stack);
    frame->type = F_FILE;
    frame->fp   = fp;
}

static void
frame_push_macro(frame_vec *frame_stack, macro *macro, token_vec_vec arguments)
{
    frame *frame;

    frame = frame_vec_push(frame_stack);
    frame->type        = F_MACRO;
    frame->macro       = macro;
    frame->replace_idx = 0;
    frame->arguments   = arguments;
}

static void
frame_push_list(frame_vec *frame_stack, token_vec tokens)
{
    frame *frame;

    frame = frame_vec_push(frame_stack);
    frame->type      = F_LIST;
    frame->tokens    = tokens;
    frame->token_idx = 0;
}

static void
frame_push_token(frame_vec *frame_stack, token token)
{
    frame *frame;

    frame = frame_vec_push(frame_stack);
    frame->type      = F_LIST;
    token_vec_init(&frame->tokens);
    token_vec_add(&frame->tokens, token);
    frame->token_idx = 0;
}

static void
frame_next_token(frame_vec *frame_stack, token *token)
{
    frame *frame;

recurse:
    frame = frame_stack->arr + frame_stack->n - 1;
    switch (frame->type) {
    case F_FILE:
        // Read token directly from lexer
        lex_next_token(frame->fp, token);
        // Remove frame on end of file
        if (token->type == TK_END_FILE)
            --frame_stack->n;
        break;
    case F_MACRO:;
        // Remove frame on the end of the replacement list
        if (frame->replace_idx == frame->macro->replacement_list.n) {
            // Remove blue pain from macro
            frame->macro->painted_blue = 0;
            --frame_stack->n;
            // Recurse to get token from the frame under the macro
            goto recurse;
        }

        // Get the next element of the macro's replacement list
        replace r = frame->macro->replacement_list.arr[frame->replace_idx++];

        switch (r.type) {
        case R_TOKEN:
            // Get token from replacement list
            *token = r.token;
            break;
        case R_PARAM:
            // Push argument list for required parameter
            frame_push_list(frame_stack, frame->arguments.arr[r.index]);
            // Recurse to get token from the new frame
            goto recurse;
        }

        break;
    case F_LIST:
        // Remove frame on the end of the token list
        if (frame->token_idx == frame->tokens.n) {
            --frame_stack->n;
            // Recurse to get token from the frame under the macro
            goto recurse;
        }

        // Get token from the token list
        *token = frame->tokens.arr[frame->token_idx++];
        break;
    }
}

static void
next_token_expand(frame_vec *frame_stack,
    macro_map *macro_database, token *token);

static void
expand_macro(frame_vec *frame_stack, macro_map *macro_database, macro *macro)
{
    token_vec     cur_arg;
    token_vec_vec arguments;
    int           paren_nest;
    token         tmp;

    // Capture arguments if the macro is function like
    if (macro->function_like) {
        // Initialize data structures
        token_vec_init(&cur_arg);
        token_vec_vec_init(&arguments);

        // Argument list needs to start with (
        next_token_expand(frame_stack, macro_database, &tmp);
        if (tmp.type != TK_LEFT_PAREN)
            cpp_err();
        paren_nest = 1;

        for (;;) {
            next_token_expand(frame_stack, macro_database, &tmp);
            switch (tmp.type) {
            case TK_END_FILE:    // Unexpected end of arguments
            case TK_END_LINE:
                cpp_err();
                break;
            case TK_COMMA:       // End of current argument
                // Ignore comma in nested parenthesis
                if (paren_nest > 1)
                    goto add_tok;
                token_vec_vec_add(&arguments, cur_arg);
                token_vec_init(&cur_arg);
                break;
            case TK_LEFT_PAREN:
                ++paren_nest;
                goto add_tok;
            case TK_RIGHT_PAREN:
                --paren_nest;
                if (paren_nest < 0) // Mismatched parenthesis
                    cpp_err();
                if (!paren_nest) {  // End of arguments
                    // Avoid adding empty argument to 0 param macro
                    if (macro->num_params || cur_arg.n)
                        token_vec_vec_add(&arguments, cur_arg);
                    goto endloop;
                }
                goto add_tok;
            default:             // Add token to argument
            add_tok:
                token_vec_add(&cur_arg, tmp);
                break;
            }
        }
        endloop:

        // Make sure the macro has the correct number of arguments
        if (macro->num_params != arguments.n)
            cpp_err();
    }

    // Paint macro blue
    macro->painted_blue = 1;

    // Push macro frame
    frame_push_macro(frame_stack, macro, arguments);
}

void
next_token_expand(frame_vec *frame_stack,
    macro_map *macro_database, token *out)
{
    token tmp;
    _Bool function_like;

recurse:

    // Read token from frame stack
    frame_next_token(frame_stack, out);

    // Non-identifiers can't expand
    if (out->type != TK_IDENTIFIER || out->no_expand)
        return;

    // Check if the macro is function like
    frame_next_token(frame_stack, &tmp);
    function_like = tmp.type == TK_LEFT_PAREN;
    frame_push_token(frame_stack, tmp);

    // Check if this is macro to be expanded
    macro *macro = macro_map_getptr(macro_database, out->data);

    if (macro) {
        if (macro->painted_blue) {
            // Mark token for no further expansion if it did not expand because
            // of self-referential macro ISO/IEC 9899 6.10.3.4
            out->no_expand = 1;
        } else if (function_like == macro->function_like) {
            printf("Expanding: %s\n", out->data);
            // Perform macro expension
            expand_macro(frame_stack, macro_database, macro);
            // Swallow identifier
            goto recurse;
        }
    }
}

static void
dir_define(frame_vec *frame_stack, macro_map *macro_database)
{
    macro *macro;
    token identifier, tmp;
    index_map param_to_idx;

    // Macro name must be an identifier
    frame_next_token(frame_stack, &identifier);
    if (identifier.type != TK_IDENTIFIER)
        cpp_err();

    // Put macro name into database and get pointer to struct
    macro = macro_map_putptr(macro_database, identifier.data);
    macro->painted_blue = 0;
    replace_vec_init(&macro->replacement_list);

    frame_next_token(frame_stack, &tmp);
    if (tmp.type == TK_LEFT_PAREN) {
        // Function like macro
        macro->function_like = 1;
        macro->num_params    = 0;

        // Need index map to parse function like replacement list
        index_map_init(&param_to_idx);

        // Capture parameter names
        for (size_t idx = 0;; ++idx) {
            frame_next_token(frame_stack, &tmp);
            switch (tmp.type) {
            case TK_IDENTIFIER:
                // Save index of parameter name
                index_map_put(&param_to_idx, tmp.data, idx);
                // Increase parameter count
                ++macro->num_params;
                // Paramter name must be followed either by a , or )
                frame_next_token(frame_stack, &tmp);
                if (tmp.type == TK_RIGHT_PAREN)
                    goto break_loop;
                else if (tmp.type != TK_COMMA)
                    cpp_err();
                break;
            case TK_RIGHT_PAREN: // End of parameter names
                goto break_loop;
            default:             // No other tokens are valid here
                cpp_err();
            }
        }
        break_loop:

        // Capture replacement list
        for (;;) {
            frame_next_token(frame_stack, &tmp);
            switch (tmp.type) {
            case TK_END_FILE:
            case TK_END_LINE:
                 // End line or end file means end of macro
                goto break_loop2;
            default:;
                // Append token or parameter index to replacement list
                replace *r = replace_vec_push(&macro->replacement_list);
                size_t param_idx;
                if (tmp.type == TK_IDENTIFIER &&
                        index_map_get(&param_to_idx, tmp.data, &param_idx)) {
                    r->type  = R_PARAM;
                    r->index = param_idx;
                } else {
                    r->type = R_TOKEN;
                    r->token = tmp;
                }
            }
        }
        break_loop2:

        // Free index map
        index_map_free(&param_to_idx);
    } else {
        // Normal macro
        macro->function_like = 0;

        for (;; frame_next_token(frame_stack, &tmp)) {
            switch (tmp.type) {
            case TK_END_FILE:
            case TK_END_LINE:
                // End line or end file means end of macro
                goto break_loop3;
            default:;
                // Append token to replacement list
                replace *r = replace_vec_push(&macro->replacement_list);
                r->type  = R_TOKEN;
                r->token = tmp;
                break;
            }
        }
        break_loop3:;
    }
}

static void
dir_undef(frame_vec *frame_stack, macro_map *macro_database)
{
    token identifier;

    // Macro name must be an identifier
    frame_next_token(frame_stack, &identifier);
    if (identifier.type != TK_IDENTIFIER)
        cpp_err();

    // Delete macro from database
    macro_map_del(macro_database, identifier.data);
}


static void
preprocess(FILE *fp)
{
    frame_vec frame_stack;
    macro_map macro_database;
    _Bool     recognize_directive;
    token     token;

    // Initialize frame stack
    frame_vec_init(&frame_stack);
    frame_push_file(&frame_stack, fp);

    // Initialize macro database
    macro_map_init(&macro_database);

    // Enable directive recognition at the start
    recognize_directive = 1;

    for (;;) {
        next_token_expand(&frame_stack, &macro_database, &token);
        switch (token.type) {
        case TK_END_FILE:
            // Exit loop on end of file
            return;
        case TK_END_LINE:
            // Newline enables directive recognition
            recognize_directive = 1;
            break;
        case TK_HASH:
            // Check if we need to recognize a directive
            if (recognize_directive) {
                // Directive name must be an identifier
                frame_next_token(&frame_stack, &token);
                if (token.type != TK_IDENTIFIER)
                    cpp_err();
                // Check for all supported directives
                if (!strcmp(token.data, "define"))
                    dir_define(&frame_stack, &macro_database);
                else if (!strcmp(token.data, "undef"))
                    dir_undef(&frame_stack, &macro_database);
                else
                    cpp_err();
                // Swallow hash token
                continue;
            }
            // FALLTHROUGH
        default:
            recognize_directive = 0;
            break;
        }

        // We need to output the token here
        output_token(&token);
    }

}

int
main(int argc, char *argv[])
{
    char *path;
    FILE *fp;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    path = argv[1];

    if (!(fp = fopen(path, "r"))) {
        perror(path);
        return 1;
    }

    preprocess(fp);
    fclose(fp);
    return 0;
}

//// CODE end
