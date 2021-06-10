// SPDX-License-Identifier: GPL-2.0-only

/*
 * Compiler driver
 */

#include <stdio.h>
#include <pp/token.h>
#include <pp/pp.h>
#include <target.h>
#include <parse/parse.h>

int main(int argc, char *argv[])
{
    PpContext *pp;
    ParseCtx *parse;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    // Setup pre-processor
    pp = pp_create(argv[1]);
    if (!pp) {
        perror(argv[1]);
        return 1;
    }

    // Setup parser
    parse = parse_create(pp);

    // Run parser (temporary API for testing)
    parse_run(parse);

    parse_free(parse);
    pp_free(pp);
}
