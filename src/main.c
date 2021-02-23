#include <stdio.h>
#include <stdlib.h>
#include "err.h"
#include "target.h"
#include "pp/token.h"
#include "pp/pp.h"
#include "parse/parse.h"

int main(int argc, char *argv[])
{
    PpContext *pp_ctx;
    ParseCtx *parse_ctx;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }

    pp_ctx = pp_create(argv[1]);
    if (!pp_ctx) {
        perror(argv[1]);
        return 1;
    }

    parse_ctx = parse_open(pp_ctx);
    parse_run(parse_ctx);

    parse_free(parse_ctx);
    pp_free(pp_ctx);
    return 0;
}
