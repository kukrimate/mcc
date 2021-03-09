#ifndef PP_H
#define PP_H

// Preprocessor context
typedef struct PpContext PpContext;

// Create a pre-processor context for a file
PpContext *pp_create(const char *path);

// Create a pre-processor context for a string
PpContext *pp_create_string(const char *string);

// Read the next expanded token from the preprocessor
Token *pp_expand(PpContext *ctx);

// Free a preprocessor context
void pp_free(PpContext *ctx);

#endif
