#ifndef __HASH_H__
#define __HASH_H__

#include "rx.h"

hash_t *hash_init (hash_func_t *hash_func, equal_func_t *equal_func);
unsigned int hash_direct_hash (void *key);
int hash_direct_equal (void *key1, void *key2);
unsigned int hash_str_hash (void *key);
int hash_str_equal (void *key1, void *key2);
int hash_index (hash_t *h, void *key, unsigned int hash);
void hash_clear (hash_t *h);
void hash_free (hash_t *h);
void hash_resize (hash_t *h, int new_allocated);
void hash_insert (hash_t *h, void *key, void *value);
void *hash_lookup (hash_t *h, void *key);

#endif

