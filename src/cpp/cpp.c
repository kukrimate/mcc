/*
 * Core of the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "macro.h"
#include "cpp.h"

/* Does this token end a directive */
#define IS_EDIR(tok) ((tok)->type == EFILE || (tok)->type == NLINE)
/* Is this token the specified punctuator */
#define IS_PUNCT(tok, str) ((tok)->type == PUNCT && !strcmp((tok)->data, str))

void
cpp_err(void)
{
	fprintf(stderr, "Pre-processor error!\n");
	exit(1);
}

static
void
include(FILE *fp)
{
	struct tok token;

	/* Read header name */
	next_token(fp, &token, 1);
	if (token.type != HNAME)
		cpp_err();
	printf("Included: %s\n", token.data);
	free(token.data);

	/* #include directive must not have more tokens */
	next_token(fp, &token, 0);
	if (!IS_EDIR(&token))
		cpp_err();
	free(token.data);
}

static
void
directive(FILE *fp)
{
	struct tok token;
	struct mdef macro;

	/* Empty directives are just ignored */
	next_token(fp, &token, 0);
	if (IS_EDIR(&token))
		return;

	/* Directive must start with an identifier */
	if (token.type != IDENT)
		cpp_err();

	/* Process directive */
	if (!strcmp(token.data, "include"))
		include(fp);
	else if (!strcmp(token.data, "define")) {
		define(fp, &macro);
		free_macro(&macro);
	} else
		cpp_err();

	free(token.data);
}

void
preprocess(FILE *fp)
{
	_Bool allow_dir;	/* Allow directives */
	struct tok token;	/* Current token */

	/* Allow pre-processing directives from the start */
	allow_dir = 1;

	for (;;) {
		next_token(fp, &token, 0);
		switch (token.type) {
		case EFILE:
			free(token.data);
			return;
		case NLINE:
			allow_dir = 1;
			break;
		case PUNCT:
			/* Found a pre-processing directive */
			if (allow_dir && !strcmp(token.data, "#")) {
				directive(fp);
				break;
			}
		default:
			/* Normal pre-processing token */
			printf("%s\n", token.data);
			allow_dir = 0;
			break;
		}
		free(token.data);
	}
}
