/*
 * Make sure our hash table works correctly
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <lib/hash.h>

int main(void)
{
    Hash *hash = hash_alloc();
    char *key;

    for (size_t i = 0; i < 1000000; ++i) {
        asprintf(&key, "key%ld", i);
        hash_insert(hash, key, (void *) i);
        assert(hash_lookup(hash, key) == (void *) i);
        if (i % 10 == 0) {
            hash_delete(hash, key);
        }
    }

    hash_free(hash);
}
