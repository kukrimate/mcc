#include <stdio.h>
#include "token.h"
#include "pp.h"

int main(int argc, char *argv[])
{
    PpContext *ctx;
    Token *tmp;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    ctx = pp_create(argv[1]);
    if (!ctx) {
        perror(argv[1]);
        return 1;
    }

    while ((tmp = pp_proc(ctx))) {
        output_token(tmp);
        free_token(tmp);
    }

    pp_free(ctx);
    return 0;
}
