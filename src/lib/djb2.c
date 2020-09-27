/*
 * Daniel Bernstein's hash function
 */

unsigned int
djb2_hash(const char *str)
{
	unsigned int hash;

	hash = 5381;
	for (; *str; ++str)
		hash = ((hash << 5) + hash) + *str;
	return hash;
}
