/*
 * C pre-processor
 */

#include <stdio.h>
#include <stdlib.h>
#include "chvec.h"
#include "mcc.h"
#include "cpp.h"

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
	for (;;) {
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

void
cpp_tokenize(void)
{
	struct chvec v;
	int ch;

	chvec_init(&v);
	while ((ch = mgetc()) != EOF) {
		if (ch == '/') {
			/* New style comment */
			if (mnext('/') == '/') {
				while (ch = mgetc(),
					ch != '\n' && ch != EOF);
				continue;
			}
			/* Old style comment */
			if (mnext('*') == '*') {
				while (ch = mgetc(),
					!(ch == '*' && mnext('/') == '/'));
				continue;
			}
		}

		switch (ch) {
		/* Whitespace */
		WHITESPACE
			continue;
		/* Identifier */
		NONDIGIT
			identifier(&v, ch);
			break;
		/* String */
		case '"':
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

		/* Print token */
		printf("%s\n", chvec_str(&v));
		/* Clear vector */
		v.n = 0;
	}
}
