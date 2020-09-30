/*
 * Compiler wide string table to save memory
 * This is essentially the same as a hash set with some extra
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <djb2.h>

/* 50 different strings are enough to parse
 * a small C program without realloc */
static size_t str_size = 50;
static size_t str_load = 0;
static char **str_arr = NULL;

__attribute__((constructor))
static
void
str_init(void)
{
	str_arr = calloc(str_size, sizeof(*str_arr));
}

__attribute__((destructor))
static
void
str_deinit(void)
{
	size_t i;

	for (i = 0; i < str_size; ++i)
		free(str_arr[i]);
	free(str_arr);
}

static
void
str_set(char *str)
{
	size_t i;

	i = djb2_hash(str) % str_size;
	while (str_arr[i])
		i = (i + 1) % str_size;
	str_arr[i] = str;
}

char *
str_getptr(char *str)
{
	size_t i;
	char *result;

	size_t oldsize;
	char **oldarr;

	i = djb2_hash(str) % str_size;
	while (str_arr[i]) {
		if (!strcmp(str_arr[i], str))
			return str_arr[i];
		i = (i + 1) % str_size;
	}

	result = str_arr[i] = strdup(str);

	if (++str_load >= (str_size * 7 / 10)) {
		oldsize = str_size;
		oldarr = str_arr;

		str_size *= 2;
		str_arr = calloc(str_size, sizeof(*str_arr));

		for (i = 0; i < oldsize; ++i)
			if (oldarr[i])
				str_set(oldarr[i]);

		free(oldarr);
	}

	return result;
}


