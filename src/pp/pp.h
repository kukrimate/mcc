// SPDX-License-Identifier: GPL-2.0-only

#ifndef PP_H
#define PP_H

//
// Opaque pre-processor context
//
typedef struct PpContext PpContext;

//
// Create a pre-processor context
//
PpContext *pp_create(void);

//
// Free a preprocessor context
//
void pp_free(PpContext *ctx);

//
// Add a search directory to the pre-processor
//
void pp_add_search_dir(PpContext *ctx, const char *dir);

//
// Push a file to the pre-processor stack
//
int pp_push_file(PpContext *ctx, const char *path);

//
// Push a string to the pre-processor stack
//
void pp_push_string(PpContext *ctx, const char *path, const char *str);

//
// Get the next pre-processed token
//
Token *pp_next(PpContext *ctx);

#endif
