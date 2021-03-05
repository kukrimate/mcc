/*
 * Standalone C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include "pp/token.h"
#include "pp/pp.h"

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

    while ((tmp = pp_expand(ctx))) {
        output_token(tmp);
        free_token(tmp);
    }

    pp_free(ctx);
}
