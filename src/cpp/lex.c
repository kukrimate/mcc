/*
 * Lexical analyzer for the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vec.h>
#include "io.h"
#include "lex.h"

VEC_DEF(char, c)

/*
 * Token type to string
 */
const char *toktype_str[] = {
	"Newline",
	"Header-name",
	"Identifier",
	"Pre-processing number",
	"Character constant",
	"String literal",
	"Punctuator"
};

/*
 * Macros to help with character recognition
 */

/* Non-digit characters used in identifiers */
#define NONDIGIT \
	case '_':case 'a':case 'b':case 'c':case 'd':case 'e':case 'f':\
	case 'g':case 'h':case 'i':case 'j':case 'k':case 'l':case 'm':\
	case 'n':case 'o':case 'p':case 'q':case 'r':case 's':case 't':\
	case 'u':case 'v':case 'w':case 'x':case 'y':case 'z':case 'A':\
	case 'B':case 'C':case 'D':case 'E':case 'F':case 'G':case 'H':\
	case 'I':case 'J':case 'K':case 'L':case 'M':case 'N':case 'O':\
	case 'P':case 'Q':case 'R':case 'S':case 'T':case 'U':case 'V':\
	case 'W':case 'X':case 'Y':case 'Z':

/* Octal digits */
#define ODIGIT \
	case '0':case '1':case '2':case '3':case '4':case '5':case '6':\
	case '7':

/* Decimal digits */
#define DIGIT \
	case '0':case '1':case '2':case '3':case '4':case '5':case '6':\
	case '7':case '8':case '9':

/* Hexadecimal digits */
#define HDIGIT \
	case '0':case '1':case '2':case '3':case '4':case '5':case '6':\
	case '7':case '8':case '9':case 'a':case 'b':case 'c':case 'd':\
	case 'e':case 'f':case 'A':case 'B':case 'C':case 'D':case 'E':\
	case 'F':

/* Characters used to build punctuators */
#define PUNCTUATOR \
	case '[':case ']':case '(':case ')':case '{':case '}':case '<':\
	case '>':case '.':case '-':case '+':case '&':case '*':case '~':\
	case '!':case '/':case '%':case '=':case '?':case ':':case ';':\
	case ',':case '^':case '|':case '#':


static
void
lex_err(void)
{
	fprintf(stderr, "Lexer error!\n");
	exit(1);
}

static
void
header_name(FILE *fp, struct cvec *v, int ch)
{
	int endch;

	endch = ch == '<' ? '>' : '"';
	cvec_add(v, ch);
	for (;;) {
		ch = mgetc(fp);
		if (ch == EOF || ch == '\n')
			lex_err();
		cvec_add(v, ch);
		if (ch == endch)
			return;
	}
}

static
void
identifier(FILE *fp, struct cvec *v, int ch)
{
	cvec_add(v, ch);
	for (;;)
		switch (mpeek(fp)) {
		/* Identifier character */
		NONDIGIT
		DIGIT
			cvec_add(v, mgetc(fp));
			break;
		/* End of identifier */
		default:
			return;
		}
}

static
void
ppnum(FILE *fp, struct cvec *v, int ch)
{
	cvec_add(v, ch);
	for (;;) {
		switch(mpeek(fp)) {
		/* Letters, numbers, _ and . */
		NONDIGIT
		DIGIT
		case '.':
			ch = mgetc(fp);
			cvec_add(v, ch);

			/* Check for exponent */
			switch (ch) {
			case 'e':
			case 'E':
			case 'p':
			case 'P':
				switch (mpeek(fp)) {
				case '+':
				case '-':
					cvec_add(v, mgetc(fp));
					break;
				}
			}
			break;
		default: /* Not a valid ppnum character */
			return;
		}
	}
}

static
void
octal(FILE *fp, struct cvec *v, int ch)
{
	ch -= '0';
	/* Octal constants only allow 3 digits max */
	switch (mpeek(fp)) {
	ODIGIT
		ch <<= 3;
		ch |= mgetc(fp) - '0';
		break;
	default:
		goto endc;
	}
	switch (mpeek(fp)) {
	ODIGIT
		ch <<= 3;
		ch |= mgetc(fp) - '0';
		break;
	}
endc:
	cvec_add(v, ch);
}

static
int
hexdigit_to_int(int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	/* FIXME: assuming a-z and A-Z being continous is non-standard */
	if (ch >= 'a' && ch <= 'f')
		return 10 + (ch - 'a');
	if (ch >= 'A' && ch <= 'F')
		return 10 + (ch - 'A');
	/* Not a valid hex digit */
	lex_err();
	return 0; /* Never reached, but makes gcc shut up */
}

static
void
hexadecimal(FILE *fp, struct cvec *v)
{
	int ch;

	ch = 0;
	/* Hex constants can be any length */
	for (;;)
		switch (mpeek(fp)) {
		HDIGIT
			ch <<= 4;
			ch |= hexdigit_to_int(mgetc(fp));
			break;
		default:
			goto endloop;
		}
endloop:
	cvec_add(v, ch);
}

static
void
escseq(FILE *fp, struct cvec *v, int ch)
{
	switch (ch = mgetc(fp)) {
	case '\'':	/* Simple escape sequences */
	case '"':
	case '?':
	case '\\':
		cvec_add(v, ch);
		break;
	case 'a':
		cvec_add(v, '\a');
		break;
	case 'b':
		cvec_add(v, '\b');
		break;
	case 'f':
		cvec_add(v, '\f');
		break;
	case 'n':
		cvec_add(v, '\n');
		break;
	case 'r':
		cvec_add(v, '\r');
		break;
	case 't':
		cvec_add(v, '\t');
		break;
	case 'v':
		cvec_add(v, '\v');
		break;
	ODIGIT		/* Octal */
		octal(fp, v, ch);
		break;
	case 'x':	/* Hexadecimal */
		hexadecimal(fp, v);
		break;
	/* Invalid escape sequence */
	default:
		lex_err();
	}
}

static
void
character(FILE *fp, struct cvec *v, int ch)
{
	cvec_add(v, ch);
	for (;;) {
		ch = mgetc(fp);
		switch (ch) {
		/* Normal character */
		default:
			cvec_add(v, ch);
			if (ch == '\'') /* End of character constant */
				return;
			break;
		/* Escape sequence */
		case '\\':
			escseq(fp, v, ch);
			break;
		/* Invalid character constant */
		case EOF:
		case '\n':
			lex_err();
		}
	}
}

static
void
string(FILE *fp, struct cvec *v, int ch)
{
	cvec_add(v, ch);
	for (;;) {
		ch = mgetc(fp);
		switch (ch) {
		/* Normal character */
		default:
			cvec_add(v, ch);
			if (ch == '"') /* End of string */
				return;
			break;
		/* Escape sequence */
		case '\\':
			escseq(fp, v, ch);
			break;
		/* Invalid string */
		case EOF:
		case '\n':
			lex_err();
		}
	}
}

static
_Bool
ifadd(FILE *fp, struct cvec *v, int ch)
{
	if (mnext(fp, ch) == ch) {
		cvec_add(v, ch);
		return 1;
	}
	return 0;
}

static
void
punctuator(FILE *fp, struct cvec *v, int ch)
{
	_Bool tmp;

	/*
		All valid punctuators in C (except digraphs which are stupid):
		[ ] ( ) { } . ->
		++ -- & * + - ~ !
		/ % << >> < > <= >= == != ^ | && ||
		? : ; ...
		= *= /= %= += -= <<= >>= &= ^= |=
		, # ##
	*/

	cvec_add(v, ch);
	switch (ch) {
	/* Always single */
	case '[':
	case ']':
	case '(':
	case ')':
	case '{':
	case '}':
	case '~':
	case '?':
	case ':':
	case ';':
	case ',':
		return;
	/* Can combine */
	case '<':
		tmp = ifadd(fp, v, '=') || (ifadd(fp, v, '<') && ifadd(fp, v, '='));
		break;
	case '>':
		tmp = ifadd(fp, v, '=') || (ifadd(fp, v, '>') && ifadd(fp, v, '='));
		break;
	case '+':
		tmp = ifadd(fp, v, '=') || ifadd(fp, v, '+');
		break;
	case '-':
		tmp = ifadd(fp, v, '=') || ifadd(fp, v, '-') || ifadd(fp, v, '>');
		break;
	case '&':
		tmp = ifadd(fp, v, '=') || ifadd(fp, v, '&');
		break;
	case '|':
		tmp = ifadd(fp, v, '=') || ifadd(fp, v, '|');
		break;
	case '!':
	case '=':
	case '^':
	case '*':
	case '/':
	case '%':
		tmp = ifadd(fp, v, '=');
		break;
	case '#':
		ifadd(fp, v, '#');
		break;
	case '.':
		tmp = ifadd(fp, v, '.') && !ifadd(fp, v, '.');
		if (tmp) /* The only valid combined . punctuator is ... */
			lex_err();
		break;
	default: /* Invalid punctuator, this should never be able to happen,
			left here to sanity check for bugs */
		lex_err();
	}

	tmp = !tmp; /* Make gcc shut up */
}

void
next_token(FILE *fp, struct tok *tok, _Bool header_mode)
{
	int ch;
	struct cvec v;

	/* Skip comments and whitespaces */
	for (;;)
		switch (ch = mgetc(fp)) {
		/* Whitespace except newline */
		case ' ':
		case '\r':
		case '\t':
		case '\v':
		case '\f':
			continue;
		case '/':
			/* New style comment */
			if (mnext(fp, '/') == '/') {
				while (ch = mgetc(fp),
					ch != EOF && ch != '\n');
				continue;
			}
			/* Old style comment */
			if (mnext(fp, '*') == '*') {
				while (ch = mgetc(fp), ch != EOF &&
					!(ch == '*' && mnext(fp, '/') == '/'));
				continue;
			}
		default:
			goto endloop;
		}
endloop:

	/* Initialize vector to store tokens */
	cvec_init(&v);

	/* If header mode is enable it overrides everything */
	if (header_mode && (ch == '<' || ch == '"')) {
		tok->type = HNAME;
		header_name(fp, &v, ch);
		goto end;

	}

	/* Wide string and char literals */
	if (ch == 'L')
		switch (mpeek(fp)) {
		case '\'':
			cvec_add(&v, 'L');
			ch = mgetc(fp);
			goto charlit;
		case '"':
			cvec_add(&v, 'L');
			ch = mgetc(fp);
			goto strlit;
		}

	/* Optional . in front of ppnums */
	if (ch == '.')
		switch (mpeek(fp)) {
		DIGIT
			cvec_add(&v, '.');
			ch = mgetc(fp);
			goto ppnum;
		}

	switch (ch) {
	/* End of file */
	case EOF:
		tok->type = EFILE;
		break;
	/* Newline */
	case '\n':
		tok->type = NLINE;
		break;
	/* Identifier */
	NONDIGIT
		tok->type = IDENT;
		identifier(fp, &v, ch);
		break;
	/* Pre-processing number */
	DIGIT
	ppnum:
		tok->type = PPNUM;
		ppnum(fp, &v, ch);
		break;
	/* Character */
	case '\'':
	charlit:
		tok->type = CHARC;
		character(fp, &v, ch);
		break;
	/* String */
	case '"':
	strlit:
		tok->type = STLIT;
		string(fp, &v, ch);
		break;
	/* Punctuator */
	PUNCTUATOR
		tok->type = PUNCT;
		punctuator(fp, &v, ch);
		break;
	/* Invalid token */
	default:
		lex_err();
	}

end:
	tok->data = cvec_arr(&v);
}
