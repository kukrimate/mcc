#ifndef LEX_H
#define LEX_H

#include "io.h"
#include "token.h"

typedef struct {
    // Underlying IO handle
    Io *io;
    // Token buffer
    VECtoken buffer;
} LexCtx;

// Initialize lexer for a file
void lex_init(LexCtx *ctx, Io *io);

// Get the next token
void lex_next(LexCtx *ctx, token *token);

// Peek at the next token
void lex_peek(LexCtx *ctx, token *token);

#endif
