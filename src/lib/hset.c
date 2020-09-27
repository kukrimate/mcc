/*
 * Hash set
 *
 * the hash set and table code are very similar, the only reason I am keeping
 * them seperate is so I can cut the hash set's memory usage in half without
 * crazy macros by only using pointer sized buckets
 */

#include <stdlib.h>
#include <string.h>
#include "djb2.h"
#include "hset.h"

/*
 * Number of initial buckets to allocate
 */
#define HSET_INITIAL 10

void
hset_init(struct hset *x)
{
	x->load = 0;
	x->size = HSET_INITIAL;
	x->arr = calloc(x->size, sizeof(*x->arr));
}

void
hset_set(struct hset *x, const char *str)
{
	size_t i, oldsize;
	const char **oldarr;

	/* Search for an empty bucket */
	i = djb2_hash(str) % x->size;
	while (x->arr[i]) {
		/* If we found the string we're done */
		if (!strcmp(x->arr[i], str))
			return;
		i = (i + 1) % x->size;
	}
	/* Write string to bucket */
	x->arr[i] = str;
	/* Increase load and resize the table if needed */
	if (++x->load >= (x->size * 7 / 10)) {
		/* We need the old table to copy values */
		oldsize = x->size;
		oldarr = x->arr;
		/* Allocate new table */
		x->size *= 2;
		x->arr = calloc(x->size, sizeof(*x->arr));
		/* Copy entries */
		for (i = 0; i < oldsize; ++i)
			if (oldarr[i])
				hset_set(x, oldarr[i]);
		/* Free old table */
		free(oldarr);
	}
}

_Bool
hset_isset(struct hset *x, const char *str)
{
	size_t i;

	i = djb2_hash(str) % x->size;
	while (x->arr[i]) {
		if (!strcmp(x->arr[i], str))
			return 1;
		i = (i + 1) % x->size;
	}
	return 0;
}
