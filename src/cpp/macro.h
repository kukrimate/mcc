#ifndef MACRO_H
#define MACRO_H

/* Replacement list entry */
struct rent {
	/* Lexer token */
	struct tok *tok;
	/* Should this token be replaced with an argument? */
	_Bool is_arg;
	/* Index of the argument to replace this token with */
	size_t arg;
};

/* Macro definition */
struct mdef {
	/* Identifier of this macro */
	const char *ident;
	/* Is this a function like macro? */
	_Bool is_flike;
	/* Replacement list for this macro */
	struct rent *rlist;
	/* All lexer tokens consumed by parsing this macro */
	struct tok *stack;
};

void
define(struct mdef *macro);

void
free_macro(struct mdef *macro);

#endif
