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
	fprintf(stderr, "Invalid input!\n");
	exit(1);
}

static
void
identifier(struct chvec *v, int ch)
{
	chvec_add(v, ch);
	for (;;) {
		ch = mgetc();
		chvec_add(v, ch);

		switch (ch) {
		/* Identifier character */
		NONDIGIT
		DIGIT
			break;
		/* End of identifier */
		default:
			return;
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
		chvec_add(v, ch);

		switch (ch) {
		/* Unterminated string */
		case EOF:
		case '\n':
			cpp_err();
		/* FIXME: add escape sequences */
		/* End of string */
		case '"':
			return;
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
			unless the compiler the lexer is broken */
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
