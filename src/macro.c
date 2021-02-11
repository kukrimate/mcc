/*
 * Macro expansion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <djb2.h>
#include <map.h>
#include <set.h>
#include <vec.h>
#include "io.h"
#include "token.h"
#include "lex.h"
#include "pp.h"
#include "macro.h"

// Vector of token lists
VEC_GEN(VECtoken, 2token)
// String to int map (for parameter indexes)
MAP_GEN(const char *, size_t, djb2_hash, !strcmp, index)

static void capture_actuals(VECframe *frame_stack, macro *macro, VEC2token *actuals)
{
    token     tmp;
    VECtoken  actual;
    int       paren_nest;

    // Argument list needs to start with (
    if (frame_next(frame_stack).type != TK_LEFT_PAREN)
        pp_err();

    // Initialize data structures
    VEC2token_init(actuals);
    VECtoken_init(&actual);

    // Start at 1 deep parenthesis
    paren_nest = 1;

    for (;;) {
        tmp = frame_next(frame_stack);
        switch (tmp.type) {
        case TK_END_FILE:
        case TK_END_LINE:
            // Unexpected end of parameters
            pp_err();
            break;
        case TK_COMMA:
            // Ignore comma in nested parenthesis
            if (paren_nest > 1)
                goto add_tok;
            // End of current parameter
            VEC2token_add(actuals, actual);
            VECtoken_init(&actual);
            break;
        case TK_LEFT_PAREN:
            ++paren_nest;
            goto add_tok;
        case TK_RIGHT_PAREN:
            --paren_nest;
            // Mismatched parenthesis
            if (paren_nest < 0)
                pp_err();
            // End of parameters
            if (!paren_nest)
                goto endloop;
            goto add_tok;
        default:
        add_tok:
            // Add token to argument
            VECtoken_add(&actual, tmp);
            break;
        }
    }
    endloop:

    // Add arguments for last paremeter,
    // avoid adding empty parameter to 0 parameter macro
    if (macro->num_params || actual.n)
        VEC2token_add(actuals, actual);

    // Make sure the macro has the correct number of parameters
    if (macro->num_params != actuals->n)
        pp_err();
}

static void expand_macro(MAPmacro *macro_database, macro *macro,
                         VEC2token *actuals, VECtoken *result)
{
    VECframe actual_stack;

    for (size_t i = 0; i < macro->replacement_list.n; ++i) {
        replace *replace = macro->replacement_list.arr + i;

        // Basic token from the replacement list becomes part of the result
        if (replace->type == R_TOKEN) {
            token tmp = replace->token;
            tmp.no_expand = 1;
            VECtoken_add(result, tmp);
            continue;
        }

        // Otherwise we need the actual for the parameter
        VECtoken *actual = actuals->arr + replace->index;

        // Now we will need to deal with the actuals in some ways
        switch (replace->type) {
        case R_TOKEN: // NOTE: not possible as we checked for it above
            abort();
        case R_PARAM_EXP:
            // The current macro can expand again during actual pre-expansion
            // Push current parameter's actuals to a new frame stack
            VECframe_init(&actual_stack);
            frame_push_list(&actual_stack, actuals->arr[replace->index]);
            // Macro expand the actuals
            for (;;) {
                token tmp = next_token_expand(&actual_stack, macro_database);
                if (tmp.type == TK_END_FILE)
                    break;
                VECtoken_add(result, tmp);
            }
            break;
        case R_PARAM_STR:
            // Create string of all tokens
            ;VECc new_str;
            VECc_init(&new_str);
            for (size_t i = 0; i < actual->n; ++i)
                stringify_token(1, actual->arr + i, &new_str);
            VECc_add(&new_str, 0);
            // Add string as a new token
            token *new_token = VECtoken_push(result);
            new_token->lwhite = 0;
            new_token->type = TK_STRING_LIT;
            new_token->data = new_str.arr;
            break;
        case R_PARAM_GLU:
            VECtoken_addall(result, actual->arr, actual->n);
            // We need to add a placemarker
            if (!actual->n)
                VECtoken_add(result, (token) { .type = TK_PLACEMARKER });
            break;
        }
    }
}

// Do glue
// for (size_t i = 0; i < result->n; ++i) {
//     token *t = result->arr + i;
//     if (t->type != TK_HASH_HASH) // non-interesting
//         continue;

//     token *left  = result->arr + i - 1;
//     token *right = result->arr + i + 1;

//     // create string from two tokens
//     VECc new_str;
//     VECc_init(&new_str);
//     stringify_token(0, left, &new_str);
//     stringify_token(0, right, &new_str);
//     VECc_add(&new_str, 0);

//     // add new token
//     Io strio;
//     strio.type = IO_STR;
//     strio.str = new_str.arr;
//     LexCtx lex;
//     lex_init(&lex, &strio);
//     size_t oldwhite = left->lwhite;
//     lex_next(&lex, left);
//     left->lwhite = oldwhite;

//     // pop unneeded tokens
//     VECtoken_pop(result, i);
//     VECtoken_pop(result, i);
// }

token next_token_expand(VECframe *frame_stack, MAPmacro *macro_database)
{
    token     tmp;
    macro     *macro;
    VEC2token actuals;
    VECtoken  result;

recurse:
    // Read token from the frame stack
    tmp = frame_next(frame_stack);

    // Return token if no macro expansion is required
    if (tmp.type != TK_IDENTIFIER || tmp.no_expand)
        return tmp;

    // See if the token is a macro name
    macro = MAPmacro_get(macro_database, tmp.data);
    if (!macro)
        return tmp;

    // We need to capture the catuals if the macro is function like
    if (macro->function_like) {
        // Eat newline
        while (frame_peek(frame_stack).type == TK_END_LINE)
            frame_next(frame_stack);
        // If there are no actuals provided, we don't expand
        if (frame_peek(frame_stack).type != TK_LEFT_PAREN)
            return tmp;
        // Capture the actuals
        capture_actuals(frame_stack, macro, &actuals);
        // Expand macro
        VECtoken_init(&result);
        expand_macro(macro_database, macro, &actuals, &result);
    } else {
        // Expand macro
        VECtoken_init(&result);
        expand_macro(macro_database, macro, NULL, &result);
    }

    // Push result after a successful expansion
    frame_push_list(frame_stack, result);
    // Continue recursively expanding
    goto recurse;
}

void dir_define(VECframe *frame_stack, MAPmacro *macro_database)
{
    macro *macro;
    token identifier, tmp;
    MAPindex param_to_idx;
    _Bool stringify;
    _Bool glue;

    // Macro name must be an identifier
    identifier = frame_next(frame_stack);
    if (identifier.type != TK_IDENTIFIER)
        pp_err();

    // Put macro name into database and get pointer to struct
    macro = MAPmacro_put(macro_database, identifier.data);
    VECreplace_init(&macro->replacement_list);
    macro->disabled = 0;

    tmp = frame_next(frame_stack);
    if (tmp.type == TK_LEFT_PAREN) {
        // Function like macro
        macro->function_like = 1;
        macro->num_params    = 0;

        // Need index map to parse function like replacement list
        MAPindex_init(&param_to_idx);

        // Capture parameter names
        for (size_t idx = 0;; ++idx) {
            tmp = frame_next(frame_stack);
            switch (tmp.type) {
            case TK_IDENTIFIER:
                // Save index of parameter name
                *MAPindex_put(&param_to_idx, tmp.data) = idx;
                // Increase parameter count
                ++macro->num_params;
                // Paramter name must be followed either by a , or )
                tmp = frame_next(frame_stack);
                if (tmp.type == TK_RIGHT_PAREN)
                    goto break_loop;
                else if (tmp.type != TK_COMMA)
                    pp_err();
                break;
            case TK_RIGHT_PAREN: // End of parameter names
                goto break_loop;
            default:             // No other tokens are valid here
                pp_err();
            }
        }
        break_loop:

        // Capture replacement list
        stringify = 0;
        glue = 0;
        for (;;) {
            tmp = frame_next(frame_stack);
            switch (tmp.type) {
            case TK_END_FILE:
            case TK_END_LINE:
                 // End line or end file means end of macro
                goto break_loop2;
            case TK_HASH:
                stringify = 1;
                break;
            case TK_HASH_HASH:;
                // Must be preceeded by something
                if (!macro->replacement_list.n)
                    pp_err();
                // Do *not* expand parameter before ## operator
                replace *r1 = VECreplace_top(&macro->replacement_list);
                if (r1->type == R_PARAM_EXP)
                    r1->type = R_PARAM_GLU;
                glue = 1;

                // Still need to add the operator as a token
                r1 = VECreplace_push(&macro->replacement_list);
                r1->type = R_TOKEN;
                r1->token = tmp;
                break;
            default:;
                // Append token or parameter index to replacement list
                replace *r2 = VECreplace_push(&macro->replacement_list);
                size_t *param_idx;
                if (tmp.type == TK_IDENTIFIER &&
                        (param_idx = MAPindex_get(&param_to_idx, tmp.data))) {
                    r2->type  = stringify ? R_PARAM_STR : (glue ? R_PARAM_GLU : R_PARAM_EXP);
                    r2->index = *param_idx;
                } else {
                    r2->type = R_TOKEN;
                    r2->token = tmp;
                }
                stringify = 0;
                glue = 0;
            }
        }
        break_loop2:

        // Glue must not be the end of the replacement list
        if (glue)
            pp_err();

        // Free index map
        MAPindex_free(&param_to_idx);
    } else {
        // Normal macro
        macro->function_like = 0;

        for (;; tmp = frame_next(frame_stack)) {
            switch (tmp.type) {
            case TK_END_FILE:
            case TK_END_LINE:
                // End line or end file means end of macro
                goto break_loop3;
            default:;
                // Append token to replacement list
                replace *r = VECreplace_push(&macro->replacement_list);
                r->type  = R_TOKEN;
                r->token = tmp;
                break;
            }
        }
        break_loop3:;
    }
}

void dir_undef(VECframe *frame_stack, MAPmacro *macro_database)
{
    token identifier;

    // Macro name must be an identifier
    identifier = frame_next(frame_stack);
    if (identifier.type != TK_IDENTIFIER)
        pp_err();

    // Delete macro from database
    MAPmacro_del(macro_database, identifier.data);
}
