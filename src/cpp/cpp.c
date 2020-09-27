/*
 * Core of the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pvec.h>
#include "lex.h"
#include "cpp.h"

static
void
cpp_err(void)
{
	fprintf(stderr, "Pre-processor error!\n");
	exit(1);
}

static
void
handle_include(struct pvec *stack)
{
	struct tok *cur;

	/* Full directive pulled */
	for (size_t i = 0; i < stack->n; ++i) {
		cur = stack->arr[i];
		printf("\t%s\n", cur->data);
	}
}

static
void
handle_directive(void)
{
	struct tok ident;

	struct pvec stack;
	struct tok *cur;

	/* Pull a token and return if it's an empty directive */
	if (!next_token(&ident, 1) || ident.type == NLINE)
		return;
	/* A directive must start with an identifier */
	if (ident.type != IDENT)
		cpp_err();
	/* Pull tokens untill the end of the directive */
	pvec_init(&stack);
	for (;;) {
		cur = malloc(sizeof(*cur));
		if (!next_token(cur, 1) || cur->type == NLINE)
			break;
		pvec_add(&stack, cur);
	}
	/* Check for supported directives */
	if (!strcmp(ident.data, "include"))
		handle_include(&stack);
	else
		cpp_err();
}

void
preprocess(void)
{
	_Bool directive;	/* Allow directives */
	struct tok token;	/* Current token */

	/* Allow pre-processing directives from the start */
	directive = 1;

	/* Pull tokens until we get a pre-processing directive */
	while (next_token(&token, 0)) {
		switch (token.type) {
		case NLINE:
			directive = 1;
			break;
		case PUNCT:
			/* Found a pre-processing directive */
			if (directive && !strcmp(token.data, "#")) {
				handle_directive();
				break;
			}
		default:
			/* Normal pre-processing token */
			printf("%s\n", token.data);
			directive = 0;
			break;
		}

	}
}
