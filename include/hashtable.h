#ifndef HASHTABLE_H_32EEA1B43F51D3
#define HASHTABLE_H_32EEA1B43F51D3

#include <stdint.h>
#include <stddef.h>

#include <vlib/std.h>

typedef uint64_t (*Hasher)(const void* data, size_t sz);

// Returns 0 if keys are equal; anything else means not equal.
typedef int (*Equaler)(const void* k1, const void* k2, size_t sz);

data(HT_Bucket) {
  struct HT_Bucket* next;
  uint64_t          hash;
  char              data[0];  // key + value
};

data(Hashtable) {
  Hasher      hasher;       // hash function
  Equaler     equaler;      // equality tester
  size_t      elemsz;       // size of each value
  size_t      keysz;        // size of each key
  double      loadfactor;   // maximum load factor before rehashing
  size_t      size;         // number of elements

  size_t      _cap;
  HT_Bucket** _buckets;
  size_t      _maxsize;     // precalculated maximum size before a rehash is needed
};

void hashtable_init7(Hashtable* ht, Hasher h, Equaler e, size_t keysz, size_t elemsz, size_t capacity, double loadfactor);
static inline void hashtable_init(Hashtable* ht, Hasher h, Equaler e, size_t keysz, size_t elemsz) {
  hashtable_init7(ht, h, e, keysz, elemsz, 7, 0.75);
}

void hashtable_close(Hashtable* ht);

// Returns the pointer to the value identified by key, or NULL if no such value was found.
void* hashtable_get(Hashtable* ht, const void* key);

// Inserts a new value into the hashtable and returns it's pointer.
// Values may not be inserted with the same key more than once.
void* hashtable_insert(Hashtable* ht, const void* key);

// Removes an entry from the hashtable.
// If oldkey is not NULL, then it will be filled with the old key data.
void hashtable_remove(Hashtable* ht, const void* key, void* oldkey);

/* Iteration */
// TODO: replace with callback-style iteration (see llist iter)

data(HT_Iter) {
  Hashtable*  ht;
  HT_Bucket*  bucket;
  unsigned    index;
};

// Initializes an iterator
void hashtable_iter(Hashtable* ht, HT_Iter* iter);

// Returns the next key from the iterator, or NULL.
// If data is not NULL then it will be set to the associated data pointer.
void* hashtable_next(HT_Iter* iter, void** data);

/* Builtin hasher/equaler functions */

uint64_t hasher_fnv64(const void*, size_t);
uint64_t hasher_fnv64str(const void*, size_t);

int equaler_str(const void*, const void*, size_t);

#endif /* HASHTABLE_H_32EEA1B43F51D3 */

