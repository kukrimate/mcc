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

// Identifier to index map, for finding arguments
MAP_GEN(const char *, size_t, djb2_hash, !strcmp, i)

typedef enum {
    R_TOK, // Replace with a token
    R_ARG, // Replace with an argument
} replace_type;

typedef struct {
    replace_type type;
    union {
        token  token; // Token to append
        size_t index; // Index of the argument
    };
} replace;

// Vector for creating the replacement list
VEC_GEN(replace, r)

typedef struct {
    // Is this macro currently callable?
    _Bool   painted_blue;
    // Is this a function like macro?
    _Bool   function_like;
    // Number of arguments (if function like)
    size_t  num_args;
    // Replacement list for each argument
    replace *replacement_list;
} macro;

// Hash map for storing macros
MAP_GEN(const char *, macro *, djb2_hash, !strcmp, m)

//// TYPES end

//// CODE start

static void
cpp_err(void)
{
    fprintf(stderr, "Preprocessor error!\n");
    exit(1);
}

static void
dir_define(FILE *fp, struct mmap *macro_database)
{
    struct imap arg_to_idx;
    struct rvec replacement_list;
    macro *macro;
    token identifier, tmp;

    // Initialize data structures
    imap_init(&arg_to_idx);
    rvec_init(&replacement_list);

    // Initialize macro struct
    macro = malloc(sizeof(macro));
    if (!macro)
        abort();
    macro->painted_blue = 0;

    // Macro name must be an identifier
    lex_next_token(fp, &identifier);
    if (identifier.type != TK_IDENTIFIER)
        cpp_err();

    // Check if this macro is function like
    lex_next_token(fp, &tmp);
    if (tmp.type == TK_LEFT_PAREN) { // Function like
        macro->function_like = 1;
        macro->num_args      = 0;

        // Capture argument names
        // NOTE: argument indexes start at 1 as libkm maps use 0
        // as the not-present marker
        for (size_t idx = 1;; ++idx) {
            lex_next_token(fp, &tmp);
            switch (tmp.type) {
            case TK_IDENTIFIER:
                // Save index for argument name
                imap_put(&arg_to_idx, tmp.data, idx);
                // Increase argument count
                ++macro->num_args;
                // Argument name followed either by a , or )
                lex_next_token(fp, &tmp);
                if (tmp.type == TK_RIGHT_PAREN)
                    goto capture_replace;
                else if (tmp.type != TK_COMMA)
                    cpp_err();
                break;
            case TK_RIGHT_PAREN: // End of argument names
                goto capture_replace;
            default:             // No other tokens are valid here
                cpp_err();
            }
        }

        // Capture replacement list
        capture_replace:
        for (;;) {
            lex_next_token(fp, &tmp);
            switch (tmp.type) {
            case TK_END_FILE: // End line or end file means end of macro
            case TK_END_LINE:
                goto done;
            default:;         // Append token or argument index to replacement list
                replace *r = rvec_push(&replacement_list);
                size_t arg_idx;
                if (tmp.type == TK_IDENTIFIER &&
                        (arg_idx = imap_get(&arg_to_idx, tmp.data))) {
                    r->type  = R_ARG;
                    r->index = arg_idx - 1;
                } else {
                    r->type = R_TOK;
                    r->token = tmp;
                }
            }
        }
    } else {                         // Normal
        macro->function_like = 0;

        for (;; lex_next_token(fp, &tmp)) {
            switch (tmp.type) {
            case TK_END_FILE: // End line or end file means end of macro
            case TK_END_LINE:
                goto done;
            default:;         // Append token to replacement list
                replace *r = rvec_push(&replacement_list);
                r->type  = R_TOK;
                r->token = tmp;
                break;
            }
        }
    }

done:
    // Free argument index map
    imap_free(&arg_to_idx);
    // Copy replacement list pointer to macro struct
    macro->replacement_list = replacement_list.arr;
    // Add macro to the macro database
    mmap_put(macro_database, identifier.data, macro);
}

static void
preprocess(FILE *fp)
{
    struct mmap macro_database;

    token token;
    _Bool recognize_directive;

    // Initialize macro database
    mmap_init(&macro_database);

    // Directives are only recognizes after newlines (and at the start)
    recognize_directive = 1;

    for (;;) {
        lex_next_token(fp, &token);
        switch (token.type) {
        case TK_END_FILE: // We're done
            return;
        case TK_END_LINE: // Newline enable directive recognition
            recognize_directive = 1;
            break;
        case TK_HASH:     // Hash might signal a directive
            if (recognize_directive) {
                lex_next_token(fp, &token);

                // Directive name must be an identifier
                if (token.type != TK_IDENTIFIER)
                    cpp_err();

                // Check for all supported directives
                if (!strcmp(token.data, "define")) // #define
                    dir_define(fp, &macro_database);
                else
                    cpp_err();
            }
            break;
        default:          // Other tokens disable directive recognition
            printf("%d\n", token.type);
            recognize_directive = 0;
            break;
        }
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
