#ifndef CPP_H
#define CPP_H

/* Replacement list entry */
struct rent {
	/* Lexer token */
	struct tok tok;
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
	/* Is this macro painted blue? */
	_Bool is_blue;
	/* Replacement list for this macro */
	struct rent *rlist;
};

/* Token source type */
enum pptype {
	PP_FILE,
	PP_MACRO,
	PP_ARRAY
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
			/* Arguments passed to this macro */
			struct tok **args;
		} macro;
		struct {
			struct tok *tokens;
			size_t tok_idx;
		} array;
	} _;
};


void
preprocess(FILE *fp);

#endif
