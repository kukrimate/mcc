/*
 * Character vector, header-only, hopefully fully inlined
 */
#ifndef CHVEC_H
#define CHVEC_H

struct chvec {
	char *arr;
	size_t len, n;
};

static inline void chvec_init(struct chvec *x)
{
	(x)->n = 0;
	(x)->len = 10;
	(x)->arr = malloc((x)->len);
}

static inline void chvec_add(struct chvec *x, int ch)
{
	if (x->len < ++x->n) {
		x->len = x->n * 2;
		x->arr = realloc(x->arr, x->len);
	}
	x->arr[x->n - 1] = ch;
}

static inline char *chvec_str(struct chvec *x)
{
	chvec_add(x, 0);
	return x->arr;
}

#endif
