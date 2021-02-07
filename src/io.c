/*
 * C pre-processor I/O layer
 */

#include <stdio.h>
#include "io.h"

/*
 * Translation "Phase 1" aka map input file to "source charset", e.g.
 * for us it means replacing CRLF (Windows) or CR (Macintosh) with LF
 */
static int mgetc_newline(FILE *fp)
{
    int ch;

    ch = fgetc(fp);

    /* Convert a CRLF or CR into an LF */
    if (ch == '\r') {
        ch = fgetc(fp);

        /* Remove the next character from the stream if it's an LF */
        if (ch != '\n') {
            ungetc(ch, fp);
            ch = '\n';
        }
    }

    return ch;
}

/*
 * Translation "Phase 2" aka line splicing, e.g. we remove \ + newline
 * This is the interface used by the C pre-processor to read characters
 */
int mgetc(FILE *fp)
{
    int ch;

    ch = mgetc_newline(fp);
    if (ch == '\\') {
        ch = mgetc_newline(fp);
        if (ch != '\n') {
            ungetc(ch, fp);
            ch = '\\';
        } else {
            ch = mgetc_newline(fp);
        }
    }

    return ch;
}

/*
 * Peek at the next character without removing it from the stream
 */
int mpeek(FILE *fp)
{
    int ch;
    ch = mgetc(fp);
    ungetc(ch, fp);
    return ch;
}

/*
 * Peek at the next character and remove it from the stream if matches want
 */
int mnext(FILE *fp, int want)
{
    int ch;
    ch = mgetc(fp);
    if (ch != want)
        ungetc(ch, fp);
    return ch;
}

/*
 * Remove want from the stream
 */
_Bool mnextstr(FILE *fp, const char *want)
{
    int ch;
    const char *cur;

    for (cur = want; *cur; ++cur) {
        ch = mgetc(fp);
        if (ch != *cur) {
            ungetc(ch, fp);
            while (cur > want)
                ungetc(*--cur, fp);
            return 0;
        }
    }

    return 1;
}
