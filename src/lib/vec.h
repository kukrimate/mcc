/*
 * Vector, aka dynamic array
 */
#ifndef VEC_H
#define VEC_H

/*
 * Define a vector type for the given base type
 */
#define VEC_DEF(type, prefix) \
\
struct prefix##vec { \
	type *arr; \
	size_t len, n; \
}; \
 \
static inline void prefix##vec_init(struct prefix##vec *x) \
{ \
	x->n = 0; \
	x->len = 10; \
	x->arr = malloc(x->len * sizeof(type)); \
} \
 \
static inline void prefix##vec_add(struct prefix##vec *x, type e) \
{ \
	if (x->len < ++x->n) { \
		x->len = x->n * 2; \
		x->arr = realloc(x->arr, x->len * sizeof(type)); \
	} \
	x->arr[x->n - 1] = e; \
} \
static inline type *prefix##vec_ptr(struct prefix##vec *x) \
{ \
	if (x->len < ++x->n) { \
		x->len = x->n * 2; \
		x->arr = realloc(x->arr, x->len * sizeof(type)); \
	} \
	return x->arr + x->n - 1; \
} \
static inline type *prefix##vec_arr(struct prefix##vec *x) \
{ \
	if (x->len < ++x->n) { \
		x->len = x->n * 2; \
		x->arr = realloc(x->arr, x->len * sizeof(type)); \
	} \
	memset(x->arr + x->n - 1, 0, sizeof(type)); \
	return x->arr; \
} \
static inline type *prefix##vec_tail(struct prefix##vec *x) \
{ \
	return x->arr + x->n - 1; \
}

#endif
