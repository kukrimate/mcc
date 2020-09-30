/*
 * Core of the C pre-processor
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include <htab.h>
#include "lex.h"
#include "cpp.h"

VEC_DEF(struct rent, r)
VEC_DEF(struct ppsrc, p)

static
void
cpp_err(void)
{
	fprintf(stderr, "Pre-processor error!\n");
	exit(1);
}

/* Pre-processor stack */
static struct pvec ppstack;

static
void
cpp_pushfile(FILE *fp)
{
	struct ppsrc *filesrc;

	filesrc = pvec_ptr(&ppstack);
	filesrc->type = PP_FILE;
	filesrc->_.file.fp = fp;
}

static
void
cpp_pushmacro(struct mdef *macro)
{
	struct ppsrc *macrosrc;

	macro->is_blue = 1;
	macrosrc = pvec_ptr(&ppstack);
	macrosrc->type = PP_MACRO;
	macrosrc->_.macro.macro = macro;
	macrosrc->_.macro.rlist_idx = 0;
}

static
void
cpp_read(struct tok *token, _Bool header_mode)
{
	struct ppsrc *src;
	struct mdef *macro;

retry:
	src = pvec_tail(&ppstack);
	switch (src->type) {
	case PP_FILE:
		next_token(src->_.file.fp, token, header_mode);
		break;
	case PP_MACRO:
		macro = src->_.macro.macro;
		if (!macro->rlist[src->_.macro.rlist_idx].tok.data) {
			macro->is_blue = 0;
			--ppstack.n;
			goto retry;
		}

		*token = macro->rlist[src->_.macro.rlist_idx].tok;
		++src->_.macro.rlist_idx;
		break;
	}
}


static
void
include(void)
{
	struct tok token;

	/* Read header name */
	cpp_read(&token, 1);
	if (token.type != HNAME)
		cpp_err();
	printf("Included: %s\n", token.data);

	/* #include directive must not have more tokens */
	cpp_read(&token, 0);
	if (token.type != EFILE && token.type != NLINE)
		cpp_err();
}

static
struct mdef *
define(void)
{
	/* Temporary variable to hold a token */
	struct tok token;

	/* Replacement list */
	struct rent *rent;
	struct rvec rlist;

	/* Hash table for finding arguments */
	struct htab args;
	size_t arg;

	/* Output macro struct */
	struct mdef *macro;

	rvec_init(&rlist);
	htab_init(&args);
	macro = malloc(sizeof(*macro));

	/* Read macro identifier */
	cpp_read(&token, 0);
	if (token.type != IDENT)
		cpp_err();

	macro->ident = token.data;
	macro->is_flike = 0;
	macro->is_blue = 0;

	/* Parse arguments for function like macro */
	cpp_read(&token, 0);
	if (IS_PUNCT(&token, "(")) {
		macro->is_flike = 1;
		for (arg = 0;;) {
			cpp_read(&token, 0);
			if (IS_PUNCT(&token, ","))	/* Ignore all , */
				continue;
			if (IS_PUNCT(&token, ")"))	/* Matching ) */
				break;
			if (token.type != IDENT)
				cpp_err();
			htab_put(&args, token.data, (void *) (1 + arg++));
		}
		cpp_read(&token, 0);
	}

	/* Construct replacement list */
	while (token.type != EFILE && token.type != NLINE) {
		rent = rvec_ptr(&rlist);
		rent->tok = token;
		rent->is_arg = 0;

		if (token.type == IDENT) {
			arg = (size_t) htab_get(&args, token.data);
			if (arg) {
				rent->is_arg = 1;
				rent->arg = arg - 1;
			}
		}

		cpp_read(&token, 0);
	}

	/* Create arrays from vectors */
	macro->rlist = rvec_arr(&rlist);

	/* Free internally used hash-table */
	free(args.arr);

	return macro;
}

static
void
free_macro(struct mdef *macro)
{
	free(macro->rlist);
}

static
void
directive(struct htab *macros)
{
	struct tok token;
	struct mdef *macro;

	/* Empty directives are just ignored */
	cpp_read(&token, 0);
	if (token.type == EFILE || token.type == NLINE)
		return;

	/* Directive must start with an identifier */
	if (token.type != IDENT)
		cpp_err();

	/* Process directive */
	if (!strcmp(token.data, "include")) {
		include();
	} else if (!strcmp(token.data, "define")) {
		macro = define();
		htab_put(macros, macro->ident, (void *) macro);
	} else {
		cpp_err();
	}
}

void
preprocess(FILE *fp)
{
	struct htab macros;	/* Macro table */
	_Bool allow_dir;	/* Allow directives */
	struct tok token;	/* Current token */
	struct mdef *macro;	/* Current macro */

	/* Create pre-processor stack and push source file */
	pvec_init(&ppstack);
	cpp_pushfile(fp);

	/* Initialize macro table */
	htab_init(&macros);
	/* Allow pre-processing directives from the start */
	allow_dir = 1;

	for (;;) {
		cpp_read(&token, 0);
		switch (token.type) {
		case EFILE:
			goto end;
		case NLINE:
			allow_dir = 1;
			break;
		case PUNCT:
			/* Found a pre-processing directive */
			if (allow_dir && !strcmp(token.data, "#")) {
				directive(&macros);
				break;
			}
		case IDENT:
			macro = (void *) htab_get(&macros, token.data);
			if (macro && !macro->is_blue) {
				cpp_pushmacro(macro);
				break;
			}
		default:
			/* Normal pre-processing token */
			printf("%s\n", token.data);
			allow_dir = 0;
			break;
		}
	}

end:
	for (size_t i = 0; i < macros.size; ++i)
		if (macros.arr[i].key) {
			free_macro((void *) macros.arr[i].val);
			free((void *) macros.arr[i].val);
		}
	free(macros.arr);
	free(ppstack.arr);
}
