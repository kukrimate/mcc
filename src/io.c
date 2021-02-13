/*
 * C pre-processor I/O layer
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "io.h"

typedef enum {
    IO_FILE,
    IO_STR,
} IoType;

struct Io {
    IoType type;
    union {
        // IO_FILE
        FILE *fp;
        // IO_STR
        const char *str;
    };
};

Io *io_open(const char *path)
{
    FILE *fp;
    Io *io;

    if (!(fp = fopen(path, "r")))
        return NULL;

    io = malloc(sizeof *io);
    io->type = IO_FILE;
    io->fp = fp;
    return io;
}

Io *io_open_string(const char *string)
{
    Io *io;

    io = malloc(sizeof *io);
    io->type = IO_STR;
    io->str = string;
    return io;
}

int io_getc(Io *io)
{
    int ch;

    switch (io->type) {
    case IO_STR:
        ch = *io->str;
        if (!ch) {
            // Translate NUL-to EOF
            ch = EOF;
        } else {
            // Advance string to next character
            ++io->str;
        }
        break;
    case IO_FILE:
        // Read character from stream
        ch = fgetc(io->fp);
        break;
    }

    return ch;
}

static void io_ungetc(int ch, Io *io)
{
    switch (io->type) {
    case IO_STR:
        // Make sure this is used correctly
        assert(*--io->str == ch);
        break;
    case IO_FILE:
        // Push character back to stream
        ungetc(ch, io->fp);
        break;
    }
}

int io_peek(Io *io)
{
    int ch;

    ch = io_getc(io);
    io_ungetc(ch, io);
    return ch;
}

_Bool io_next(Io *io, int want)
{
    int ch;

    ch = io_getc(io);
    if (ch != want) {
        io_ungetc(ch, io);
        return 0;
    }
    return 1;
}

_Bool io_nextstr(Io *io, const char *want)
{
    int ch;
    const char *cur;

    for (cur = want; *cur; ++cur) {
        ch = io_getc(io);
        if (ch != *cur) {
            io_ungetc(ch, io);
            while (cur > want)
                io_ungetc(*--cur, io);
            return 0;
        }
    }
    return 1;
}

void io_close(Io *io)
{
    if (io->type == IO_FILE) {
        // Close underlying stdio handle
        fclose(io->fp);
    }
    free(io);
}
