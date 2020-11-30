/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	char *path;
	FILE *fp;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		return 1;
	}

	path = argv[1];

	if (!(fp = fopen(path, "r"))) {
		perror(path);
		return 1;
	}

	fclose(fp);
	return 0;
}
