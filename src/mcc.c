/*
 * mcc main file
 */

#include <stdio.h>
#include <stdlib.h>
#include "cpp/cpp.h"

int
main(int argc, char *argv[])
{
	FILE *fp;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	fp = fopen(argv[1], "r");
	if (!fp) {
		perror(argv[1]);
		return 1;
	}

	preprocess(fp);
	fclose(fp);
	return 0;
}
