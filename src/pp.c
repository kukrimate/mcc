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
#include "io.h"
#include "token.h"
#include "lex.h"
#include "pp.h"

void pp_err(void)
{
    fprintf(stderr, "Preprocessor error!\n");
    exit(1);
}

void frame_push_file(VECframe *frame_stack, Io *io)
{
    frame *frame;

    frame = VECframe_push(frame_stack);
    frame->type = F_LEXER;
    lex_init(&frame->ctx, io);
}

void frame_push_list(VECframe *frame_stack, macro *source, VECtoken tokens)
{
    frame *frame;

    // disable source macro
    if (source)
        source->enabled = 0;

    frame = VECframe_push(frame_stack);
    frame->type      = F_LIST;
    frame->source    = source;
    frame->tokens    = tokens;
    frame->token_idx = 0;
}

token frame_next(VECframe *frame_stack)
{
recurse:
    // Return end of file if the frame stack is empty
    if (!frame_stack->n)
        return (token) { .type = TK_END_FILE };

    // Get most recent frame from the stack
    frame *frame = VECframe_top(frame_stack);

    switch (frame->type) {
    case F_LEXER:
        // Read token directly from lexer
        ;token tmp;
        lex_next(&frame->ctx, &tmp);
        // Remove frame on end of file
        if (tmp.type == TK_END_FILE) {
            --frame_stack->n;
            goto recurse;
        }
        return tmp;
    case F_LIST:
        // Remove frame on the end of the token list
        if (frame->token_idx == frame->tokens.n) {
            if (frame->source)
                frame->source->enabled = 1;
            --frame_stack->n;
            goto recurse;
        }
        // Get token from the token list
        return frame->tokens.arr[frame->token_idx++];
    }

    // non-reachable
    abort();
}

token frame_peek(VECframe *frame_stack)
{
recurse:
    // Return end of file if the frame stack is empty
    if (!frame_stack->n)
        return (token) { .type = TK_END_FILE };

    // Get most recent frame from the stack
    frame *frame = VECframe_top(frame_stack);

    switch (frame->type) {
    case F_LEXER:
        // Read token directly from lexer
        ;token tmp;
        lex_peek(&frame->ctx, &tmp);
        // Remove frame on end of file
        if (tmp.type == TK_END_FILE) {
            --frame_stack->n;
            goto recurse;
        }
        return tmp;
    case F_LIST:
        // Remove frame on the end of the token list
        if (frame->token_idx == frame->tokens.n) {
            if (frame->source)
                frame->source->enabled = 1;
            --frame_stack->n;
            goto recurse;
        }
        // Get token from the token list
        return frame->tokens.arr[frame->token_idx];
    }

    // non-reachable
    abort();
}

static void preprocess(Io *io)
{
    VECframe frame_stack;
    MAPmacro macro_database;
    _Bool     recognize_directive;
    token     token;

    // Initialize frame stack
    VECframe_init(&frame_stack);
    frame_push_file(&frame_stack, io);

    // Initialize macro database
    MAPmacro_init(&macro_database);

    // Enable directive recognition at the start
    recognize_directive = 1;

    for (;;) {
        token = next_token_expand(&frame_stack, &macro_database);
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
                token = frame_next(&frame_stack);
                if (token.type != TK_IDENTIFIER)
                    pp_err();
                // Check for all supported directives
                if (!strcmp(token.data, "define"))
                    dir_define(&frame_stack, &macro_database);
                else if (!strcmp(token.data, "undef"))
                    dir_undef(&frame_stack, &macro_database);
                else
                    pp_err();
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
    Io *io;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    if (!(io = mopen(argv[1]))) {
        perror(argv[1]);
        return 1;
    }

    preprocess(io);
    mclose(io);
    return 0;
}
