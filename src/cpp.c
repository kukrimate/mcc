/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <set.h>
#include <map.h>
#include <djb2.h>
#include "lex.h"

//// TYPES start

// Vector of tokens
VEC_GEN(token, token_)

// Vector of token lists
VEC_GEN(token_vec, token_vec_)

// String set as a hideset
SET_GEN(const char *, djb2_hash, !strcmp, str_)

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
    // Return end of file if the frame stack is empty
    if (!frame_stack->n) {
        token->type = TK_END_FILE;
        return;
    }

    // Get most recent frame from the stack
    frame = frame_stack->arr + frame_stack->n - 1;

    switch (frame->type) {
    case F_FILE:
        // Read token directly from lexer
        lex_next_token(frame->fp, token);
        // Remove frame on end of file
        if (token->type == TK_END_FILE) {
            --frame_stack->n;
            goto recurse;
        }
        break;
    case F_LIST:
        // Remove frame on the end of the token list
        if (frame->token_idx == frame->tokens.n) {
            --frame_stack->n;
            goto recurse;
        }
        // Get token from the token list
        *token = frame->tokens.arr[frame->token_idx++];
        break;
    }
}

static token
frame_peek(frame_vec *frame_stack)
{
    token token;

    do {
        frame_next_token(frame_stack, &token);
    } while (token.type == TK_END_LINE);
    frame_push_token(frame_stack, token);
    return token;
}

// Capture arguments for each parameter
static void
capture_arguments(frame_vec *frame_stack, macro *macro, token_vec_vec *parameters)
{
    token     tmp;
    token_vec arguments;
    int       paren_nest;

    // Argument list needs to start with (
    frame_next_token(frame_stack, &tmp);
    if (tmp.type != TK_LEFT_PAREN)
        cpp_err();

    // Initialize data structures
    token_vec_vec_init(parameters);
    token_vec_init(&arguments);

    // Start at 1 deep parenthesis
    paren_nest = 1;

    for (;;) {
        frame_next_token(frame_stack, &tmp);
        switch (tmp.type) {
        case TK_END_FILE:
        case TK_END_LINE:
            // Unexpected end of parameters
            cpp_err();
            break;
        case TK_COMMA:
            // Ignore comma in nested parenthesis
            if (paren_nest > 1)
                goto add_tok;
            // End of current parameter
            token_vec_vec_add(parameters, arguments);
            token_vec_init(&arguments);
            break;
        case TK_LEFT_PAREN:
            ++paren_nest;
            goto add_tok;
        case TK_RIGHT_PAREN:
            --paren_nest;
            // Mismatched parenthesis
            if (paren_nest < 0)
                cpp_err();
            // End of parameters
            if (!paren_nest)
                goto endloop;
            goto add_tok;
        default:
        add_tok:
            // Add token to argument
            token_vec_add(&arguments, tmp);
            break;
        }
    }
    endloop:

    // Add arguments for last paremeter,
    // avoid adding empty parameter to 0 parameter macro
    if (macro->num_params || arguments.n)
        token_vec_vec_add(parameters, arguments);

    // Make sure the macro has the correct number of parameters
    if (macro->num_params != parameters->n)
        cpp_err();
}

static void
next_token_expand(frame_vec *frame_stack, macro_map *macro_database, token *out);

static _Bool
expand_macro(frame_vec *frame_stack, macro_map *macro_database,
    token identifier, str_set *hideset, token_vec *result)
{
    macro         *macro;
    token_vec_vec parameters;
    frame_vec     param_stack;

    // See if the token can be expanded
    if (identifier.type != TK_IDENTIFIER
            || identifier.no_expand
            || !(macro = macro_map_getptr(macro_database, identifier.data)))
        return 0;

    // Ignore function like macro name without parenthesis
    if (macro->function_like && frame_peek(frame_stack).type != TK_LEFT_PAREN)
        return 0;

    // Add indetifier to the hideset
    str_set_set(hideset, identifier.data);

    // Capture arguments if the macro is function like
    if (macro->function_like)
        capture_arguments(frame_stack, macro, &parameters);

    // Iterate through replacement list
    for (size_t i = 0; i < macro->replacement_list.n; ++i) {
        replace replace_entry = macro->replacement_list.arr[i];
        switch (replace_entry.type) {
        case R_TOKEN:;
            token tmp = replace_entry.token;
            if (tmp.type == TK_IDENTIFIER && str_set_isset(hideset, tmp.data))
                tmp.no_expand = 1;
            token_vec_add(result, tmp);
            break;
        case R_PARAM:
            // Create frame stack for parameter
            frame_vec_init(&param_stack);
            frame_push_list(&param_stack, parameters.arr[replace_entry.index]);
            // Read all frames from the argument
            for (;;) {
                next_token_expand(&param_stack, macro_database, &tmp);
                if (tmp.type == TK_END_FILE)
                    break;
                if (tmp.type == TK_IDENTIFIER && str_set_isset(hideset, tmp.data))
                    tmp.no_expand = 1;
                token_vec_add(result, tmp);
            }
            break;
        }
    }

    return 1;
}

void
next_token_expand(frame_vec *frame_stack, macro_map *macro_database, token *out)
{
    str_set   hideset;
    token_vec result;

    str_set_init(&hideset);
recurse:
    frame_next_token(frame_stack, out);
    token_vec_init(&result);
    if (expand_macro(frame_stack, macro_database, *out, &hideset, &result)) {
        frame_push_list(frame_stack, result);
        goto recurse;
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
