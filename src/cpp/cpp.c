/*
 * Core of the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <htab.h>
#include "lex.h"
#include "cpp.h"

VEC_DEF(struct tok, t)
VEC_DEF(struct rent, r)
VEC_DEF(struct ppsrc, p)

static
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
	if (token.type != EFILE && token.type != NLINE)
		cpp_err();
	free(token.data);
}

static
struct mdef *
define(FILE *fp)
{
	/* Replacement list */
	struct rent *rent;
	struct rvec rlist;

	/* Token stack */
	struct tok *token;
	struct tvec stack;

	/* Hash table for finding arguments */
	struct htab args;
	size_t arg;

	/* Output macro struct */
	struct mdef *macro;

	rvec_init(&rlist);
	tvec_init(&stack);
	htab_init(&args);
	macro = malloc(sizeof(*macro));

	/* Read macro identifier */
	token = tvec_ptr(&stack);
	next_token(fp, token, 0);
	if (token->type != IDENT)
		cpp_err();

	macro->ident = token->data;
	macro->is_flike = 0;

	/* Parse arguments for function like macro */
	token = tvec_ptr(&stack);
	next_token(fp, token, 0);
	if (IS_PUNCT(token, "(")) {
		macro->is_flike = 1;
		for (arg = 0;;) {
			token = tvec_ptr(&stack);
			next_token(fp, token, 0);
			if (IS_PUNCT(token, ","))	/* Ignore all , */
				continue;
			if (IS_PUNCT(token, ")"))	/* Matching ) */
				break;
			if (token->type != IDENT)
				cpp_err();
			htab_put(&args, token->data, (void *) (1 + arg++));
		}
		token = tvec_ptr(&stack);
		next_token(fp, token, 0);
	}

	/* Construct replacement list */
	while (token->type != EFILE && token->type != NLINE) {
		rent = rvec_ptr(&rlist);
		rent->tok = token;
		rent->is_arg = 0;

		if (token->type == IDENT) {
			arg = (size_t) htab_get(&args, token->data);
			if (arg) {
				rent->is_arg = 1;
				rent->arg = arg - 1;
			}
		}

		token = tvec_ptr(&stack);
		next_token(fp, token, 0);
	}

	/* Create arrays from vectors */
	macro->rlist = rvec_arr(&rlist);
	macro->stack = tvec_arr(&stack);


	/* Free internally used hash-table */
	free(args.arr);

	return macro;
}

static
void
free_macro(struct mdef *macro)
{
	struct tok *token;

	/* Free tokens */
	for (token = macro->stack; token->data; ++token)
		free(token->data);

	/* Free arrays */
	free(macro->rlist);
	free(macro->stack);
}

static
void
directive(FILE *fp, struct htab *macros)
{
	struct tok token;
	struct mdef *macro;

	/* Empty directives are just ignored */
	next_token(fp, &token, 0);
	if (token.type == EFILE || token.type == NLINE)
		return;

	/* Directive must start with an identifier */
	if (token.type != IDENT)
		cpp_err();

	/* Process directive */
	if (!strcmp(token.data, "include")) {
		include(fp);
	} else if (!strcmp(token.data, "define")) {
		macro = define(fp);
		htab_put(macros, macro->ident, (void *) macro);
	} else {
		cpp_err();
	}

	free(token.data);
}

void
preprocess(FILE *fp)
{
	struct pvec ppstack;	/* Token sources */
	struct htab macros;	/* Macro table */
	_Bool allow_dir;	/* Allow directives */
	struct tok token;	/* Current token */
	size_t i;

	pvec_init(&ppstack);
	htab_init(&macros);

	/* Allow pre-processing directives from the start */
	allow_dir = 1;

	for (;;) {
		next_token(fp, &token, 0);
		switch (token.type) {
		case EFILE:
			free(token.data);
			goto end;
		case NLINE:
			allow_dir = 1;
			break;
		case PUNCT:
			/* Found a pre-processing directive */
			if (allow_dir && !strcmp(token.data, "#")) {
				directive(fp, &macros);
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

end:
	free(ppstack.arr);
	for (i = 0; i < macros.size; ++i)
		if (macros.arr[i].key) {
			free_macro((void *) macros.arr[i].val);
			free((void *) macros.arr[i].val);
		}
	free(macros.arr);
}
