#ifndef IO_H
#define IO_H

typedef enum {
    IO_STR,
    IO_FILE,
} IoType;

typedef struct {
    IoType type;
    union {
        // IO_STR
        const char *str;
        // IO_FILE
        FILE *fp;
    };
} Io;

int mgetc(Io *io);

int mpeek(Io *io);

_Bool mnext(Io *io, int want);

_Bool mnextstr(Io *io, const char *want);

#endif
