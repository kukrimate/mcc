/*
 * C pre-processor I/O layer
 */

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

Io *mopen(const char *path)
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

Io *mopen_string(const char *string)
{
    Io *io;

    io = malloc(sizeof *io);
    io->type = IO_STR;
    io->str = string;
    return io;
}

static int io_getc(Io *io)
{
    int ch;

    switch (io->type) {
    case IO_STR:
        ch = *io->str++;
        return ch ? ch : EOF;
    case IO_FILE:
        return fgetc(io->fp);
    }

    // never reached
    return EOF;
}

static void io_ungetc(int ch, Io *io)
{
    switch (io->type) {
    case IO_STR:
        --io->str;
        break;
    case IO_FILE:
        ungetc(ch, io->fp);
        break;
    }
}

/*
 * Translation "Phase 1" aka map input file to "source charset", e.g.
 * for us it means replacing CRLF (Windows) or CR (Macintosh) with LF
 */
static int mgetc_newline(Io *io)
{
    int ch;

    ch = io_getc(io);

    /* Convert a CRLF or CR into an LF */
    if (ch == '\r') {
        ch = io_getc(io);

        /* Remove the next character from the stream if it's an LF */
        if (ch != '\n') {
            io_ungetc(ch, io);
            ch = '\n';
        }
    }

    return ch;
}

/*
 * Translation "Phase 2" aka line splicing, e.g. we remove \ + newline
 * This is the interface used by the C pre-processor to read characters
 */
int mgetc(Io *io)
{
    int ch;

    ch = mgetc_newline(io);
    if (ch == '\\') {
        ch = mgetc_newline(io);
        if (ch != '\n') {
            io_ungetc(ch, io);
            ch = '\\';
        } else {
            ch = mgetc_newline(io);
        }
    }

    return ch;
}

/*
 * Peek at the next character without removing it from the stream
 */
int mpeek(Io *io)
{
    int ch;

    ch = mgetc(io);
    io_ungetc(ch, io);

    return ch;
}

/*
 * Remove want from the stream
 */
_Bool mnext(Io *io, int want)
{
    int ch;

    ch = mgetc(io);
    if (ch != want) {
        io_ungetc(ch, io);
        return 0;
    }

    return 1;
}

/*
 * Remove want from the stream
 */
_Bool mnextstr(Io *io, const char *want)
{
    int ch;
    const char *cur;

    for (cur = want; *cur; ++cur) {
        ch = mgetc(io);
        if (ch != *cur) {
            io_ungetc(ch, io);
            while (cur > want)
                io_ungetc(*--cur, io);
            return 0;
        }
    }

    return 1;
}

void mclose(Io *io)
{
    if (io->type == IO_FILE)
        fclose(io->fp);
    free(io);
}
