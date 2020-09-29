#ifndef LEX_H
#define LEX_H

/*
 * Standard C pre-processing tokens plus newline
 * The reason we have newline in the token stream is to avoid forcing
 * the pre-processor to use a side-channel to retrieve when a macro ends
 */
enum toktype {
	EFILE,	/* End of file */
	NLINE,	/* End of line */
	HNAME,	/* Header-name */
	IDENT,	/* Identifier */
	PPNUM,	/* Pre-processing number */
	CHARC,	/* Character constant */
	STLIT,	/* String literal */
	PUNCT,	/* Punctuator */
};

/*
 * Token type to string
 */
extern
const char *toktype_str[];

/*
 * Represents a token as fed into the pre-processor
 */
struct tok {
	/* Type of token */
	enum toktype type;
	/* Token data */
	char *data;
};

/*
 * Get the next pre-processing token
 * If header_mode is true, the lexer will recognize header name tokens
 * Note that lexer having to recognize header names is an utter violation of
 * layering, but it's C design mistake that can't really be avoided
 */
void
next_token(FILE *fp, struct tok *tok, _Bool header_mode);

#endif
