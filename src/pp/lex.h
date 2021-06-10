// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEX_H
#define LEX_H

typedef struct LexCtx LexCtx;

//
// Open  a lexer context for a file
//
LexCtx *lex_open_file(const char *file);

//
// Open a lexer context for an in-memory string
//
LexCtx *lex_open_string(const char *str);

//
// Free the lexer context
//
void lex_free(LexCtx *ctx);

//
// Obtain the next token from the lexer
//
Token *lex_next(LexCtx *ctx, _Bool want_header_name);

#endif
