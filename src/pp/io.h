// SPDX-License-Identifier: GPL-2.0-only

#ifndef IO_H
#define IO_H

typedef struct Io Io;

// Open an Io handle to a file on disk
Io *io_open(const char *path);
// Open an Io handle to a string in memory
Io *io_open_string(const char *string);

// Operations needed by the lexer
int io_getc(Io *io);
int io_peek(Io *io);
_Bool io_next(Io *io, int want);
_Bool io_nextstr(Io *io, const char *want);

// Close an Io handle
void io_close(Io *io);

#endif
