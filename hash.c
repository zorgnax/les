// This is a simple hash table implementation.

#include "rx.h"
#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hash_t *hash_init (hash_func_t *hash_func, equal_func_t *equal_func) {
    hash_t *h = malloc(sizeof(hash_t));
    h->allocated = 10;
    h->count = 0;
    h->defined = calloc(1, h->allocated * sizeof(int));
    h->hashes = malloc(h->allocated * sizeof(int));
    h->keys = malloc(h->allocated * sizeof(void *));
    h->values = malloc(h->allocated * sizeof(void *));
    h->hash_func = hash_func;
    h->equal_func = equal_func;
    return h;
}

unsigned int hash_direct_hash (void *key) {
    unsigned int *hash = (unsigned int*) key;
    return (*hash);
}

int hash_direct_equal (void *key1, void *key2) {
    return key1 == key2;
}

// This is the "djb" hash, originally created by Daniel Bernstein.
// It will hash a string.
unsigned int hash_str_hash (void *key) {
    const signed char *p;
    unsigned int hash = 5381;
    for (p = key; *p; p += 1) {
        hash = (hash << 5) + hash + *p;
    }
    return hash;
}

int hash_str_equal (void *key1, void *key2) {
    int retval = strcmp(key1, key2) == 0;
    return retval;
}

int hash_index (hash_t *h, void *key, unsigned int hash) {
    int index = hash % h->allocated;
    while (1) {
        if (!h->defined[index]) {
            return index;
        }
        if (h->hashes[index] == hash && h->equal_func(h->keys[index], key)) {
            return index;
        }
        index = (index + 1) % h->allocated;
    }
    return 0;
}

// This will clear the elements of the hash, but keep the memory.
void hash_clear (hash_t *h) {
    if (h->count == 0) {
        return;
    }
    for (int i = 0; i < h->allocated; i += 1) {
        h->defined[i] = 0;
    }
    h->count = 0;
}

void hash_free (hash_t *h) {
    free(h->defined);
    free(h->hashes);
    free(h->keys);
    free(h->values);
    free(h);
}

void hash_resize (hash_t *h, int new_allocated) {
    int allocated = h->allocated;
    int *defined = h->defined;
    unsigned int *hashes = h->hashes;
    void **keys = h->keys;
    void **values = h->values;
    h->allocated = new_allocated;
    h->defined = calloc(1, new_allocated * sizeof(int));
    h->hashes = malloc(new_allocated * sizeof(int));
    h->keys = malloc(new_allocated * sizeof(void *));
    h->values = malloc(new_allocated * sizeof(void *));
    for (int i = 0; i < allocated; i += 1) {
        if (!defined[i]) {
            continue;
        }
        unsigned int hash = hashes[i];
        void *key = keys[i];
        void *value = values[i];
        int index = hash_index(h, key, hash);
        h->defined[index] = 1;
        h->hashes[index] = hash;
        h->keys[index] = key;
        h->values[index] = value;
    }
    free(defined);
    free(hashes);
    free(keys);
    free(values);
}

void hash_insert (hash_t *h, void *key, void *value) {
    unsigned int hash = h->hash_func(key);
    int index = hash_index(h, key, hash);
    if (h->defined[index]) {
        h->values[index] = value;
    } else {
        if (h->count * 2 >= h->allocated) {
            hash_resize(h, h->allocated * 2);
            index = hash_index(h, key, hash);
        }
        h->defined[index] = 1;
        h->hashes[index] = hash;
        h->keys[index] = key;
        h->values[index] = value;
        h->count += 1;
    }
}

void *hash_lookup (hash_t *h, void *key) {
    unsigned int hash = h->hash_func(key);
    int index = hash_index(h, key, hash);
    void *value = NULL;
    if (h->defined[index]) {
        value = h->values[index];
    }
    return value;
}

