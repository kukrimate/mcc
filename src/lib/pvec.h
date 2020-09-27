/*
 * Pointer vector
 */
#ifndef PVEC_H
#define PVEC_H

struct pvec {
	void **arr;
	size_t len, n;
};

static inline void pvec_init(struct pvec *x)
{
	x->n = 0;
	x->len = 10;
	x->arr = malloc(x->len * sizeof(*x->arr));
}

static inline void pvec_add(struct pvec *x, void *ptr)
{
	if (x->len < ++x->n) {
		x->len = x->n * 2;
		x->arr = realloc(x->arr, x->len * sizeof(*x->arr));
	}
	x->arr[x->n - 1] = ptr;
}

static inline void *pvec_pop(struct pvec *x)
{
	if (!x->n)
		return NULL;
	return x->arr[x->n--];
}

#endif
