#ifndef ERR_H
#define ERR_H

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

static inline NORETURN void pp_err(const char *err)
{
    fflush(stdout);
    fprintf(stderr, "\nPreprocessor error: %s\n", err);
    exit(1);
}

#endif
