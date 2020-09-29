/*
 * Pre-processor macro handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <htab.h>
#include "lex.h"
#include "cpp.h"
#include "macro.h"

VEC_DEF(struct tok, t)	/* Token list */
VEC_DEF(struct rent, r)	/* Replacement list */

void
define(FILE *fp, struct mdef *macro)
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

	rvec_init(&rlist);
	tvec_init(&stack);
	htab_init(&args);

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
}

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
