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
 \
static inline type prefix##vec_pop(struct prefix##vec *x) \
{ \
	return x->arr[x->n--]; \
} \
\
static inline type *prefix##vec_str(struct prefix##vec *x) \
{ \
	memset(x->arr + x->n++, 0, sizeof(type)); \
	return x->arr; \
}


#endif
