/*
 * Lexical analyzer for the C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include "../chvec.h"
#include "../mcc.h"
#include "lex.h"

/*
 * Macros to help with character recognition
 */

/* ASCII whitespace character */
#define WHITESPACE \
	case ' ':case '\n':case '\r':case '\t':case '\v':case '\f':

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
cpp_err(void)
{
	fprintf(stderr, "Lexer error!\n");
	exit(1);
}

static
void
identifier(struct chvec *v, int ch)
{
	chvec_add(v, ch);
	for (;;)
		switch (mpeek()) {
		/* Identifier character */
		NONDIGIT
		DIGIT
			chvec_add(v, mgetc());
			break;
		/* End of identifier */
		default:
			return;
		}
}

static
void
ppnum(struct chvec *v, int ch)
{
	chvec_add(v, ch);
	for (;;) {
		switch(mpeek()) {
		/* Letters, numbers, _ and . */
		NONDIGIT
		DIGIT
		case '.':
			ch = mgetc();
			chvec_add(v, ch);

			/* Check for exponent */
			switch (ch) {
			case 'e':
			case 'E':
			case 'p':
			case 'P':
				switch (mpeek()) {
				case '+':
				case '-':
					chvec_add(v, mgetc());
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
octal(struct chvec *v, int ch)
{
	ch -= '0';
	/* Octal constants only allow 3 digits max */
	switch (mpeek()) {
	ODIGIT
		ch <<= 3;
		ch |= mgetc() - '0';
		break;
	default:
		goto endc;
	}
	switch (mpeek()) {
	ODIGIT
		ch <<= 3;
		ch |= mgetc() - '0';
		break;
	}
endc:
	chvec_add(v, ch);
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
	cpp_err();
	return 0; /* Never reached, but makes gcc shut up */
}

static
void
hexadecimal(struct chvec *v)
{
	int ch;

	ch = 0;
	/* Hex constants can be any length */
	for (;;)
		switch (mpeek()) {
		HDIGIT
			ch <<= 4;
			ch |= hexdigit_to_int(mgetc());
			break;
		default:
			goto endloop;
		}
endloop:
	chvec_add(v, ch);
}

static
void
escseq(struct chvec *v, int ch)
{
	switch (ch = mgetc()) {
	case '\'':	/* Simple escape sequences */
	case '"':
	case '?':
	case '\\':
		chvec_add(v, ch);
		break;
	case 'a':
		chvec_add(v, '\a');
		break;
	case 'b':
		chvec_add(v, '\b');
		break;
	case 'f':
		chvec_add(v, '\f');
		break;
	case 'n':
		chvec_add(v, '\n');
		break;
	case 'r':
		chvec_add(v, '\r');
		break;
	case 't':
		chvec_add(v, '\t');
		break;
	case 'v':
		chvec_add(v, '\v');
		break;
	ODIGIT		/* Octal */
		octal(v, ch);
		break;
	case 'x':	/* Hexadecimal */
		hexadecimal(v);
		break;
	/* Invalid escape sequence */
	default:
		cpp_err();
	}
}

static
void
character(struct chvec *v, int ch)
{
	chvec_add(v, ch);
	for (;;) {
		ch = mgetc();
		switch (ch) {
		/* Normal character */
		default:
			chvec_add(v, ch);
			if (ch == '\'') /* End of character constant */
				return;
			break;
		/* Escape sequence */
		case '\\':
			escseq(v, ch);
			break;
		/* Invalid character constant */
		case EOF:
		case '\n':
			cpp_err();
		}
	}
}

static
void
string(struct chvec *v, int ch)
{
	chvec_add(v, ch);
	for (;;) {
		ch = mgetc();
		switch (ch) {
		/* Normal character */
		default:
			chvec_add(v, ch);
			if (ch == '"') /* End of string */
				return;
			break;
		/* Escape sequence */
		case '\\':
			escseq(v, ch);
			break;
		/* Invalid string */
		case EOF:
		case '\n':
			cpp_err();
		}
	}
}

static
_Bool
ifadd(struct chvec *v, int ch)
{
	if (mnext(ch) == ch) {
		chvec_add(v, ch);
		return 1;
	}
	return 0;
}

static
void
punctuator(struct chvec *v, int ch)
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

	chvec_add(v, ch);
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
		tmp = ifadd(v, '=') || (ifadd(v, '<') && ifadd(v, '='));
		break;
	case '>':
		tmp = ifadd(v, '=') || (ifadd(v, '>') && ifadd(v, '='));
		break;
	case '+':
		tmp = ifadd(v, '=') || ifadd(v, '+');
		break;
	case '-':
		tmp = ifadd(v, '=') || ifadd(v, '-') || ifadd(v, '>');
		break;
	case '&':
		tmp = ifadd(v, '=') || ifadd(v, '&');
		break;
	case '|':
		tmp = ifadd(v, '=') || ifadd(v, '|');
		break;
	case '!':
	case '=':
	case '^':
	case '*':
	case '/':
	case '%':
		tmp = ifadd(v, '=');
		break;
	case '#':
		ifadd(v, '#');
		break;
	case '.':
		tmp = ifadd(v, '.') && !ifadd(v, '.');
		if (tmp) /* The only valid combined . punctuator is ... */
			cpp_err();
		break;
	default: /* Invalid punctuator, this should never be able to happen,
			left here to sanity check for bugs */
		cpp_err();
	}

	tmp = !tmp; /* Make gcc shut up */
}

_Bool
next_token(struct tok *tok, _Bool header_mode)
{
	int ch;
	struct chvec v;

	/* Skip comments and whitespaces */
	for (;;)
		switch (ch = mgetc()) {
		WHITESPACE
			continue;
		case '/':
			/* New style comment */
			if (mnext('/') == '/') {
				while (ch = mgetc(),
					ch != EOF && ch != '\n');
				continue;
			}
			/* Old style comment */
			if (mnext('*') == '*') {
				while (ch = mgetc(), ch != EOF &&
					!(ch == '*' && mnext('/') == '/'));
				continue;
			}
		default:
			goto endloop;
		}
endloop:
	if (ch == EOF)
		return 0;

	/* Initialize vector to store tokens */
	chvec_init(&v);

	/* Wide string and char literals */
	if (ch == 'L')
		switch (mpeek()) {
		case '\'':
			chvec_add(&v, 'L');
			ch = mgetc();
			goto charlit;
		case '"':
			chvec_add(&v, 'L');
			ch = mgetc();
			goto strlit;
		}

	/* Optional . in front of ppnums */
	if (ch == '.')
		switch (mpeek()) {
		DIGIT
			chvec_add(&v, '.');
			ch = mgetc();
			goto ppnum;
		}

	switch (ch) {
	/* Identifier */
	NONDIGIT
		identifier(&v, ch);
		break;
	/* Pre-processing number */
	DIGIT
	ppnum:
		ppnum(&v, ch);
		break;
	/* Character */
	case '\'':
	charlit:
		character(&v, ch);
		break;
	/* String */
	case '"':
	strlit:
		string(&v, ch);
		break;
	/* Punctuator */
	PUNCTUATOR
		punctuator(&v, ch);
		break;
	/* Invalid token */
	default:
		cpp_err();
	}

	tok->data = chvec_str(&v);
	return 1;
}
