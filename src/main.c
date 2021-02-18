#include <stdio.h>
#include "io.h"
#include "token.h"
#include "lex.h"
#include "pp.h"

int main(int argc, char *argv[])
{
    Io *io;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    if (!(io = io_open(argv[1]))) {
        perror(argv[1]);
        return 1;
    }

    preprocess(io);

    io_close(io);
    return 0;
}
