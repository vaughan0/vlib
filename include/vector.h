#ifndef VECTOR_H_B37F00869202FC
#define VECTOR_H_B37F00869202FC

#include <assert.h>
#include <stddef.h>

#include <vlib/std.h>
#include <vlib/resource.h>

data(Vector) {
  size_t    size;   // number of elements
  size_t    elemsz; // size of each element
  char*     _data;  // data where elements are stored
  size_t    _cap;   // maximum number of elements that data can hold
};

void  vector_init(Vector* v, size_t elemsz, size_t capacity);
void  vector_close(Vector* v);

// Sets the Vector's capacity to `capacity` if it is lower.
void  vector_reserve(Vector* v, size_t capacity);
// Grows the Vector's capacity to meet the specified requirements, and grow fast
// enough to provide amortized constant time (ie exponentially).
void  vector_grow(Vector* v, size_t require);

static inline void vector_clear(Vector* v) {
  v->size = 0;
}

/* Returns a pointer to the element at index. Index must be within [0, v->size) */
static inline void* vector_get(Vector* v, unsigned index) {
  assert(index < v->size);
  return v->_data + (index * v->elemsz);
}

/* Increments the vector's size and returns a pointer to the new last element. */
void* vector_push(Vector* v);

#define vector_back(v) (vector_get((v), (v)->size-1))

/* Removes the last element from the vector and decrement's its size. */
void vector_pop(Vector* v);

data(AutoVector) {
  ResourceManager*  manager;
  Vector            v[1];
};

void    autovector_init(AutoVector* self, ResourceManager* manager);
void    autovector_close(AutoVector* self);
void    autovector_reset(AutoVector* self);

void*   autovector_push(AutoVector* self);

#endif /* VECTOR_H_B37F00869202FC */

