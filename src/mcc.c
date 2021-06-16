// SPDX-License-Identifier: GPL-2.0-only

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <lex/token.h>
#include <pp/pp.h>
#include <target.h>
#include <parse/parse.h>

static void do_preprocess(PpContext *pp)
{
    Token *tmp;
    // Read then output all tokens from the pre-processor
    while ((tmp = pp_expand(pp))) {
        output_token(tmp);
        free_token(tmp);
    }
    // Output a newline after the last token
    putchar('\n');
}

static void do_compile(PpContext *pp)
{
    ParseCtx *parse = parse_create(pp);
    parse_run(parse);
    parse_free(parse);
}

int main(int argc, char *argv[])
{
    PpContext *pp = pp_create();
    pp_add_search_dir(pp, "include");
    pp_add_search_dir(pp, "/usr/include");
    pp_add_search_dir(pp, "/usr/include/x86_64-linux-gnu");
    pp_add_search_dir(pp, "/usr/local/include");

    int opt;
    _Bool eflag = 0;

    while ((opt = getopt(argc, argv, "I:Eh")) != -1)
        switch (opt) {
        case 'I':
            pp_add_search_dir(pp, optarg);
            break;
        case 'E':
            eflag = 1;
            break;
        case 'h':
        default:
            goto print_usage;
        }

    if (optind >= argc) {
print_usage:
        fprintf(stderr, "Usage: %s [-I IDIR] [-h] FILE\n", argv[0]);
        goto err;
    }

    if (pp_push_file(pp, argv[optind]) < 0) {
        perror(argv[optind]);
        goto err;
    }

    if (eflag)
        do_preprocess(pp);
    else
        do_compile(pp);

    int result = 0;
err:
    result = 1;
    pp_free(pp);
    return result;
}
