#ifndef HSET_H
#define HSET_H

struct hset {
	const char **arr;
	size_t size, load;
};

void
hset_init(struct hset *x);

void
hset_set(struct hset *x, const char *str);

_Bool
hset_isset(struct hset *x, const char *str);

#endif
