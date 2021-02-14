#define
#define
#define
#define
hash_hash # ## #
mkstr(a) # a
in_between(a) mkstr(a)
join(c, d) in_between(c hash_hash d)
char p[] = join(x, y);
