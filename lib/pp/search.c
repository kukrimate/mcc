/*
 * Header file lookup
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "io.h"
#include "search.h"

// System header directories
static const char *system_header_dirs[] = {
    "/usr/include",
    "/usr/include/x86_64-linux-gnu",
    "/usr/local/include",
    NULL
};

Io *open_system_header(const char *name)
{
    char path[PATH_MAX];
    Io   *io;

    for (const char **dir = system_header_dirs; *dir; ++dir) {
        snprintf(path, sizeof path, "%s/%s", *dir, name);
        printf("%s\n", path);
        if ((io = io_open(path)))
            return io;
    }

    return NULL;
}

Io *open_local_header(const char *name)
{
    Io *io;

    // Retry failed local header as a system one
    if (!(io = io_open(name)))
        return open_system_header(name);
    return io;
}
