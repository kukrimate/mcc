/*
 * Hash table
 *
 * This is very simple hash table, but performs reasonably well for the usecase
 * It uses Daniel Bernstein's hash function, open addressing for collision
 * resolution and the table is load limited to 70%
 * Deleting entries will only be implemented if a use case arises inside mcc as
 * this is not meant to be a general purpose hash table
 */

#include <stdlib.h>
#include <string.h>
#include "djb2.h"
#include "htab.h"

/*
 * Number of initial buckets to allocate
 */
#define HTAB_INITIAL 10

void
htab_init(struct htab *x)
{
	x->load = 0;
	x->size = HTAB_INITIAL;
	x->arr = calloc(x->size, sizeof(*x->arr));
}

void
htab_put(struct htab *x, const char *key, const char *val)
{
	size_t i, oldsize;
	struct hent *oldarr;

	/* Search for an empty bucket, or one with the same key */
	i = djb2_hash(key) % x->size;
	while (x->arr[i].key) {
		if (!strcmp(x->arr[i].key, key))
			break;
		i = (i + 1) % x->size;
	}
	/* Write key and value to the bucket */
	x->arr[i].key = key;
	x->arr[i].val = val;
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
			if (oldarr[i].key)
				htab_put(x, oldarr[i].key, oldarr[i].val);
		/* Free old table */
		free(oldarr);
	}
}

const char *
htab_get(struct htab *x, const char *key)
{
	size_t i;
	i = djb2_hash(key) % x->size;
	while (x->arr[i].key) {
		if (!strcmp(x->arr[i].key, key))
			return x->arr[i].val;
		i = (i + 1) % x->size;
	}
	return NULL;
}
