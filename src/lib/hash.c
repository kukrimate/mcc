/*
 * Open-addressed, hash lookup table, ideas from CPython
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

// Minimum table size
#define MINSIZE 8

// Calculate new size of the table
static uint32_t newsize(uint32_t used)
{
    uint32_t newsize = MINSIZE;
    while (newsize < 3 * used)
        newsize <<= 1;
    return newsize;
}

// Maximum load for a given table size
static uint32_t maxload(uint32_t size)
{
    return size * 2 / 3;
}

// Hash functions for strings
static uint32_t djb2_hash(const char *str)
{
    uint32_t hash = 5381;
    for (; *str; ++str) {
        hash = (hash << 5) + hash + *str;
    }
    return hash;
}

typedef struct Entry Entry;

struct Entry {
    // Hash of the key
    uint32_t hash;
    // Key that hashed here
    const char *key;
    // Stored data for the key
    void *data;
    // Backwards and forwards links
    Entry *prev, *next;
};

struct Hash {
    // # of used buckets
    uint32_t used;
    // # of buckets
    uint32_t size;
    // Hash lookup array
    Entry **arr;
    // Linked list of entries
    Entry *head;
};

Hash *hash_alloc(void)
{
    Hash *self = malloc(sizeof *self);
    // Start with 0 used entries
    self->used = 0;
    // Create array
    self->size = MINSIZE;
    self->arr = calloc(self->size, sizeof *self->arr);
    // Mark entry list empty
    self->head = NULL;
    return self;
}

void hash_free(Hash *self)
{
    for (Entry *entry = self->head; entry; ) {
        Entry *next = entry->next;
        free(entry);
        entry = next;
    }
    free(self->arr);
    free(self);
}

// Two (hopefully) invalid pointers to indicate unused buckets
#define BUCKET_EMPTY ((Entry *) 0ULL)
#define BUCKET_DUMMY ((Entry *) ~0ULL)

static void rehash(Hash *self)
{
    // Free old lookup array
    free(self->arr);
    // Create new lookup array
    self->size = newsize(self->used);
    self->arr = calloc(self->size, sizeof *self->arr);

    // Add entries to the new table
    for (Entry *entry = self->head; entry; entry = entry->next) {
        uint32_t perturb = entry->hash;
        uint32_t i = entry->hash % self->size;
        Entry **bucket;

        for (;;) {
            bucket = self->arr + i;

            // Write new entry to bucket
            if (*bucket == BUCKET_EMPTY) {
                *bucket = entry;
                return;
            }

            // Move to next bucket
            perturb >>= 5;
            i = (i * 5 + perturb + 1) % self->size;
        }
    }
}

static Entry *newentry(Hash *self, uint32_t hash, const char *key, void *data)
{
    // Create new entry
    Entry *new = malloc(sizeof *new);
    new->hash = hash;
    new->key = key;
    new->data = data;
    // Insert new entry before head
    new->prev = NULL;
    new->next = self->head;
    if (self->head)
        self->head->prev = new;
    self->head = new;
    // Return address of new entry
    return new;
}

static void delentry(Hash *self, Entry *entry)
{
    if (self->head == entry)
        self->head = entry->next;
    if (entry->prev)
        entry->prev->next = entry->next;
    if (entry->next)
        entry->next->prev = entry->prev;
    free(entry);
}

void hash_insert(Hash *self, const char *key, void *data)
{
    if (self->used >= maxload(self->size))
        rehash(self);

    uint32_t hash = djb2_hash(key);
    uint32_t perturb = hash;
    uint32_t i = hash % self->size;
    Entry **bucket;

    for (;;) {
        bucket = self->arr + i;

        // Write new entry to bucket
        if (*bucket == BUCKET_EMPTY || *bucket == BUCKET_DUMMY) {
            *bucket = newentry(self, hash,key, data);
            ++self->used;
            return;
        }

        // Overwrite data in matching entry
        if ((*bucket)->hash == hash && strcmp((*bucket)->key, key) == 0) {
            (*bucket)->data = data;
            return;
        }

        // Move to next bucket
        perturb >>= 5;
        i = (i * 5 + perturb + 1) % self->size;
    }
}

void *hash_lookup(Hash *self, const char *key)
{
    uint32_t hash = djb2_hash(key);
    uint32_t perturb = hash;
    uint32_t i = hash % self->size;

    Entry **bucket;

    for (;;) {
        bucket = self->arr + i;

        // No need to probe further
        if (*bucket == BUCKET_EMPTY)
            return NULL;

        // Delete entry from matching bucket
        if (*bucket != BUCKET_DUMMY
                && (*bucket)->hash == hash
                && strcmp((*bucket)->key, key) == 0) {
            return (*bucket)->data;
        }

        // Move to next bucket
        perturb >>= 5;
        i = (i * 5 + perturb + 1) % self->size;
    }
}

void hash_delete(Hash *self, const char *key)
{
    uint32_t hash = djb2_hash(key);
    uint32_t perturb = hash;
    uint32_t i = hash % self->size;

    Entry **bucket;

    for (;;) {
        bucket = self->arr + i;

        // No need to probe further
        if (*bucket == BUCKET_EMPTY)
            return;

        // Delete entry from matching bucket
        if (*bucket != BUCKET_DUMMY
                && (*bucket)->hash == hash
                && strcmp((*bucket)->key, key) == 0) {
            delentry(self, *bucket);
            *bucket = BUCKET_DUMMY;
            return;
        }

        // Move to next bucket
        perturb >>= 5;
        i = (i * 5 + perturb + 1) % self->size;
    }
}
