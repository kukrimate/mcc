// SPDX-License-Identifier: GPL-2.0-only

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

    // Read then output all tokens from the pre-processor
    while ((tmp = pp_expand(ctx))) {
        output_token(tmp);
        free_token(tmp);
    }

    // Output a newline after the last token
    putchar('\n');

    pp_free(ctx);
}
