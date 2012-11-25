
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <vlib/hashtable.h>

void hashtable_init7(Hashtable* ht, Hasher hasher, Equaler equaler, size_t keysz, size_t elemsz, size_t cap, double loadfactor) {
  assert(keysz > 0);
  assert(elemsz > 0);
  assert(cap > 0);
  assert(loadfactor > 0);
  assert(hasher != NULL);
  assert(equaler != NULL);

  ht->hasher = hasher;
  ht->equaler = equaler;
  ht->elemsz = elemsz;
  ht->keysz = keysz;
  ht->loadfactor = loadfactor;
  ht->size = 0;
  
  ht->_maxsize = (size_t)(cap * loadfactor);
  ht->_cap = cap;
  ht->_buckets = malloc(sizeof(HT_Bucket*) * cap);
  memset(ht->_buckets, 0, sizeof(HT_Bucket*) * cap);
}
void hashtable_close(Hashtable* ht) {
  for (unsigned i = 0; i < ht->_cap; i++) {
    HT_Bucket *b, *tmp;
    for (b = ht->_buckets[i]; b; b = tmp) {
      tmp = b->next;
      free(b);
    }
  }
  free(ht->_buckets);
}

static inline HT_Bucket* lookup(Hashtable* ht, const void* key, HT_Bucket*** _prev) {
  uint64_t hash = ht->hasher(key, ht->keysz);
  unsigned index = hash % ht->_cap;
  HT_Bucket** prev = &ht->_buckets[index];
  for (HT_Bucket* b = ht->_buckets[index]; b; prev = &b->next, b = b->next) {
    if (b->hash == hash && ht->equaler(b->data, key, ht->keysz) == 0) {
      if (_prev) *_prev = prev;
      return b;
    }
  }
  return NULL;
}

// Inserts a new key and returns the new bucket for the value
static HT_Bucket* insert(Hashtable* ht, HT_Bucket** buckets, size_t cap, uint64_t hash) {
  unsigned index = hash % cap;
  HT_Bucket** ptr = &buckets[index];
  while (*ptr != NULL) {
    ptr = &((*ptr)->next);
  }
  HT_Bucket* new = malloc(sizeof(HT_Bucket) + ht->keysz + ht->elemsz);
  new->next = NULL;
  new->hash = hash;
  *ptr = new;
  return new;
}

static inline void check_loadfactor(Hashtable* ht) {
  if (ht->size >= ht->_maxsize) {
    // Rehash
    size_t newcap = ht->_cap * 2;
    HT_Bucket** newbuckets = malloc(sizeof(HT_Bucket*) * newcap);
    memset(newbuckets, 0, sizeof(HT_Bucket*) * newcap);
    // Iterate through all old buckets
    for (unsigned i = 0; i < ht->_cap; i++) {
      HT_Bucket *old, *tmp;
      for (old = ht->_buckets[i]; old; old = tmp) {
        tmp = old->next;
        HT_Bucket* new = insert(ht, newbuckets, newcap, old->hash);
        memcpy(new->data, old->data, ht->keysz + ht->elemsz);
        free(old);
      }
    }
    // Update ht
    free(ht->_buckets);
    ht->_buckets = newbuckets;
    ht->_cap = newcap;
    ht->_maxsize = (size_t)(ht->_cap * ht->loadfactor);
  }
}

void* hashtable_get(Hashtable* ht, const void* key) {
  HT_Bucket* b = lookup(ht, key, NULL);
  return b ? (b->data + ht->keysz) : NULL;
}

void* hashtable_insert(Hashtable* ht, const void* key) {
  assert(hashtable_get(ht, key) == NULL);
  check_loadfactor(ht);
  uint64_t hash = ht->hasher(key, ht->keysz);
  HT_Bucket* new = insert(ht, ht->_buckets, ht->_cap, hash);
  memcpy(new->data, key, ht->keysz);
  ht->size++;
  return new->data + ht->keysz;
}

void hashtable_remove(Hashtable* ht, const void* key, void* oldkey) {
  HT_Bucket** prev;
  HT_Bucket* b = lookup(ht, key, &prev);
  assert(b);
  if (oldkey) {
    memcpy(oldkey, b->data, ht->keysz);
  }
  // Remove bucket from the linked list
  *prev = b->next;
  free(b);
  ht->size--;
}

/* Iteration */

void hashtable_iter(Hashtable* ht, HT_Iter* iter) {
  iter->ht = ht;
  iter->bucket = NULL;
  iter->index = 0;
}

void* hashtable_next(HT_Iter* iter, void** data) {
  while (!iter->bucket) {
    if (iter->index == iter->ht->_cap)
      return NULL;
    iter->bucket = iter->ht->_buckets[iter->index++];
  }
  if (data) *data = iter->bucket->data + iter->ht->keysz;
  void* key = iter->bucket->data;
  iter->bucket = iter->bucket->next;
  return key;
}

/* Hashers */

uint64_t hasher_fnv64(const void* _data, size_t sz) {
  const char* data = _data;
  uint64_t hash = 14695981039346656037UL;
  for (unsigned i = 0; i < sz; i++) {
    unsigned octet = (unsigned)data[i] & 0xFF;
    hash ^= octet;
    hash *= 1099511628211;
  }
  return hash;
}
uint64_t hasher_fnv64str(const void* ptr, size_t sz) {
  const char* str = *(const char**)ptr;
  size_t len = strlen(str);
  return hasher_fnv64(str, len);
}

int equaler_str(const void* a, const void* b, size_t sz) {
  const char* sa = *(const char**)a;
  const char* sb = *(const char**)b;
  return (sa == sb) ? 0 : strcmp(sa, sb);
}
