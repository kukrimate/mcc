/*
 * Character vector
 */
#ifndef CVEC_H
#define CVEC_H

struct cvec {
	char *arr;
	size_t len, n;
};

static inline void cvec_init(struct cvec *x)
{
	x->n = 0;
	x->len = 10;
	x->arr = malloc(x->len);
}

static inline void cvec_add(struct cvec *x, int ch)
{
	if (x->len < ++x->n) {
		x->len = x->n * 2;
		x->arr = realloc(x->arr, x->len);
	}
	x->arr[x->n - 1] = ch;
}

static inline char *cvec_str(struct cvec *x)
{
	cvec_add(x, 0);
	return x->arr;
}

#endif
