#ifndef IO_H
#define IO_H

typedef struct Io Io;

// Open an Io handle to a file on disk
Io *mopen(const char *path);
// Open an Io handle to a string in memory
Io *mopen_string(const char *string);

// Operations needed by the lexer
int mgetc(Io *io);
int mpeek(Io *io);
_Bool mnext(Io *io, int want);
_Bool mnextstr(Io *io, const char *want);

// Close an Io handle
void mclose(Io *io);

#endif
