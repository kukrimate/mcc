#ifndef IO_H
#define IO_H

int mgetc(FILE *fp);

int mpeek(FILE *fp);

int mnext(FILE *fp, int want);

_Bool mnextstr(FILE *fp, const char *want);

#endif
