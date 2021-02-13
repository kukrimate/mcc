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
    frame->type       = F_LEXER;
    frame->io         = io;
    frame->last_valid = 0;
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
        // Return saved token
        if (frame->last_valid) {
            frame->last_valid = 0;
            return frame->last;
        }
        // Read token directly from lexer
        token tmp = lex_next(frame->io);
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
        if (frame->last_valid)
            return frame->last;
        frame->last_valid = 1;
        frame->last = lex_next(frame->io);
        // Remove frame on end of file
        if (frame->last.type == TK_END_FILE) {
            --frame_stack->n;
            goto recurse;
        }
        return frame->last;
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
    _Bool    first;
    token    token;

    // Initialize frame stack
    VECframe_init(&frame_stack);
    frame_push_file(&frame_stack, io);

    // Initialize macro database
    MAPmacro_init(&macro_database);

    // Enable directive recognition at the start
    first = 1;

    for (;; first = 0) {
        token = next_token_expand(&frame_stack, &macro_database);
        switch (token.type) {
        case TK_END_FILE:
            // Exit loop on end of file
            return;
        case TK_HASH:
            // Check if we need to recognize a directive
            if (first || token.lnew) {
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

                // Don't output anything after processing directive
                continue;
            }
        default:
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

    if (!(io = io_open(argv[1]))) {
        perror(argv[1]);
        return 1;
    }

    preprocess(io);
    io_close(io);
    return 0;
}
