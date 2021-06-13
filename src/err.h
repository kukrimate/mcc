// SPDX-License-Identifier: GPL-2.0-only

#ifndef ERR_H
#define ERR_H

#include <stdarg.h>

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

static inline NORETURN void mcc_err(const char *err, ...)
{
    // Make sure everything was printed in the stdout buffer
    fflush(stdout);

    // Print error message to stderr
    fputs("\nError: ", stderr);
    va_list ap;
    va_start(ap, err);
    vfprintf(stderr, err, ap);
    va_end(ap);
    fputc('\n', stderr);

    // Exit
    exit(1);
}

#endif
