/*
 * Open-addressed, hash lookup table
 */

#ifndef HASH_H
#define HASH_H

typedef struct Hash Hash;

// Create a new table
Hash *hash_alloc(void);

// Destroy table
void hash_free(Hash *self);

// Insert into table
void hash_insert(Hash *self, const char *key, void *data);

// Lookup key in table
void *hash_lookup(Hash *self, const char *key);

// Delete key from table
void hash_delete(Hash *self, const char *key);

#endif
