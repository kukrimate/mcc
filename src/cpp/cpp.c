/*
 * Core of the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
VEC_DEF(void *, p)
#include <htab.h>
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
handle_include(struct htab *macros, struct pvec *stack)
{
	struct tok *hname;

	/* Include can only have one header name token */
	if (stack->n != 1 || (hname = stack->arr[0], hname->type != HNAME))
		cpp_err();

	printf("Included: %s\n", hname->data);
}

struct macro {
	/* Is this a function like macro? */
	_Bool funclike;
	/* Number of parameters of a function like macro */
	size_t param_cnt;
	/* Replacement list of this macro */
	struct token **tokens;
};

/* Does this token match a specified punctuator */
#define IS_PUNCT(tok, str) (tok->type == PUNCT && !strcmp(tok->data, str))

static
void
handle_define(struct htab *macros, struct pvec *stack)
{
	struct tok *tmp;
	struct macro *macro;	/* Current macro */
	struct pvec params;	/* Parameter names */
	size_t rlist, i, j;

	/* A macro must have an identifier */
	if (stack->n < 1 || (tmp = stack->arr[0], tmp->type != IDENT))
		cpp_err();

	macro = calloc(1, sizeof(*macro));

	/* Function like macro? */
	if (stack->n >= 2 && (tmp = stack->arr[1], IS_PUNCT(tmp, "("))) {
		free(tmp->data);
		macro->funclike = 1;
		pvec_init(&params);

		for (i = 2;; ++i) {
			tmp = stack->arr[i];

			/* Must end with ), otherwise the macro is invalid */
			if (i == stack->n)
				cpp_err();

			/* End of function like macro */
			if (IS_PUNCT(tmp, ")")) {
				free(tmp->data);
				break;
			}

			/* Skip all commas in the parameter list, not strictly
			 * complient, but works with all complient code and is
			 * faster */
			if (IS_PUNCT(tmp, ",")) {
				free(tmp->data);
				continue;
			}

			/* All macro parameters must be identifiers */
			if (tmp->type != IDENT)
				cpp_err();

			/* Store parameter name */
			pvec_add(&params, tmp->data);
			/* Increase macro parameter count */
			++macro->param_cnt;
		}

		/* Replacement list starts at i + 1 */
		rlist = i + 1;

		/* Modify replacement list */
		for (i = rlist; i < stack->n; ++i) {
			tmp = stack->arr[i];
			/* Found parameter in the replacement list */
			// for (j = 0; j < params.n; ++j)
			// 	if (!strcmp(tmp->data, params.arr[j])) {
			// 		tmp->type = MARG;
			// 		tmp->data = (char *) j;
			// 	}
		}

		/* We don't need the parameter names anymore */
		for (i = 0; i < params.n; ++i)
			free(params.arr[i]);
		free(params.arr);
	} else {
		/* Non-function like replacement list always starts at 1 */
		rlist = 1;
	}

	/* Store replacement list */
	if (rlist < stack->n) {
		macro->tokens = calloc(stack->n - rlist, sizeof(*macro->tokens));
		memcpy(macro->tokens,
			&stack->arr[rlist],
			(stack->n - rlist) * sizeof(*macro->tokens));
	}

	/* Store macro */
	tmp = stack->arr[0];
	htab_put(macros, tmp->data, (const char *) macro);
}

static
void
handle_directive(struct htab *macros)
{
	struct tok ident;
	void (*handler)(struct htab *macros, struct pvec *stack);
	struct pvec stack;
	struct tok *cur;

	/* Pull a token and return if it's an empty directive */
	if (!next_token(&ident, 1) || ident.type == NLINE)
		return;

	/* A directive must start with an identifier */
	if (ident.type != IDENT)
		cpp_err();

	/* Check for supported directives */
	if (!strcmp(ident.data, "include"))
		handler = handle_include;
	else if (!strcmp(ident.data, "define"))
		handler = handle_define;
	else
		cpp_err();

	/* Pull tokens untill the end of the directive */
	pvec_init(&stack);
	for (;;) {
		cur = malloc(sizeof(*cur));
		if (!next_token(cur, 1) || cur->type == NLINE)
			break;
		pvec_add(&stack, cur);
	}
	handler(macros, &stack);
}

void
preprocess(void)
{
	_Bool directive;	/* Allow directives */
	struct htab macros;	/* Defined macros */
	struct tok token;	/* Current token */

	/* Allow pre-processing directives from the start */
	directive = 1;
	/* Create hash-table for macros */
	htab_init(&macros);

	/* Pull tokens until we get a pre-processing directive */
	while (next_token(&token, 0)) {
		switch (token.type) {
		case NLINE:
			directive = 1;
			break;
		case PUNCT:
			/* Found a pre-processing directive */
			if (directive && !strcmp(token.data, "#")) {
				handle_directive(&macros);
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
