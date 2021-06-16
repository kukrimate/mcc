// SPDX-License-Identifier: GPL-2.0-only

#ifndef LEX_H
#define LEX_H

typedef struct LexCtx LexCtx;

//
// Open a lexer context for a file
//
LexCtx *lex_open_file(const char *path);

//
// Open a lexer context for an in-memory string
//
LexCtx *lex_open_string(const char *path, const char *str);

//
// Get the currently lexed file's path
//
const char *lex_path(LexCtx *ctx);

//
// Get the current line in the file
//
size_t lex_line(LexCtx *ctx);

//
// Free the lexer context
//
void lex_free(LexCtx *ctx);

//
// Obtain the next token from the lexer
//
Token *lex_next(LexCtx *ctx);

#endif
