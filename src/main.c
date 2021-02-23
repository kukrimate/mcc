#include <stdio.h>
#include <stdlib.h>
#include "err.h"
#include "pp/token.h"
#include "pp/io.h"
#include "pp/lex.h"
#include "pp/cexpr.h"

int main(int argc, char *argv[])
{
    Io *io;
    Token *head, **tail;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    io = io_open(argv[1]);
    if (!io) {
        perror(argv[1]);
        return 1;
    }

    head = NULL;
    tail = &head;
    while ((*tail = lex_next(io, 0))) {
        if ((*tail)->type != TK_END_LINE)
            tail = &(*tail)->next;
        else
            *tail = NULL;
    }

    printf("%ld\n", eval_cexpr(head));

    io_close(io);
    return 0;
}
