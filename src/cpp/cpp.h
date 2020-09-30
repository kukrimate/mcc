#ifndef CPP_H
#define CPP_H

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

/* Token source type */
enum pptype {
	PP_FILE,
	PP_MACRO
};

/* Token source */
struct ppsrc {
	enum pptype type;
	union {
		struct {
			FILE *fp;
		} file;
		struct {
			struct mdef *macro;
			size_t rlist_idx;
		} macro;
	} _;
};


void
preprocess(FILE *fp);

#endif
