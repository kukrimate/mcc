/*
 * Preprocessor
 */

#include <stdio.h>
#include <stdlib.h>
#include "lex.h"

int
main(int argc, char *argv[])
{
	char *path;
	FILE *fp;
	token token;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		return 1;
	}

	path = argv[1];

	if (!(fp = fopen(path, "r"))) {
		perror(path);
		return 1;
	}

	for (;;) {
		next_token(fp, &token);
		if (!token.type)
			break;

		switch (token.type) {
		case TK_END_LINE:
			break;
		case TK_IDENTIFIER:
		case TK_PP_NUMBER:
		case TK_CHAR_CONST:
		case TK_STRING_LIT:
			printf("%s\n", token.data);
			break;
		default:
			printf("Punct: %x\n", token.type);
		}
	}

	fclose(fp);
	return 0;
}
