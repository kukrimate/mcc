/*
 * mcc main file
 */

#include <stdio.h>
#include "cpp.h"
#include "mcc.h"

static FILE *fp;

/*
 * Translation "Phase 1" aka map input file to "source charset", e.g.
 * for us it means replacing CRLF (Windows) or CR (Macintosh) with LF
 */
static
int
_mgetc(void)
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
int
mgetc(void)
{
	int ch;

	ch = _mgetc();
	if (ch == '\\') {
		ch = _mgetc();
		if (ch != '\n') {
			ungetc(ch, fp);
			ch = '\\';
		} else {
			ch = _mgetc();
		}
	}

	return ch;
}

int
mpeek(void)
{
	int ch;
	ch = mgetc();
	ungetc(ch, fp);
	return ch;
}

int
mnext(int want)
{
	int ch;
	ch = mgetc();
	if (ch != want)
		ungetc(ch, fp);
	return ch;
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		perror(argv[1]);
		return 1;
	}

	cpp_tokenize();

	fclose(fp);
	return 0;
}
