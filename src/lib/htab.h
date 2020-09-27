#ifndef HTAB_H
#define HTAB_H

struct hent {
	const char *key, *val;
};

struct htab {
	struct hent *arr;
	size_t size, load;
};

void
htab_init(struct htab *x);

void
htab_put(struct htab *x, const char *key, const char *val);

const char *
htab_get(struct htab *x, const char *key);

#endif
