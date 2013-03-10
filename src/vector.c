
#include <stdlib.h>
#include <assert.h>

#include <vlib/vector.h>
#include <vlib/error.h>

void vector_init(Vector* v, size_t elemsz, size_t cap) {
  assert(cap > 0);
  v->size = 0;
  v->elemsz = elemsz;
  v->_cap = cap;
  v->_data = NULL;
  v->_data = malloc(elemsz * cap);
}
void vector_close(Vector* v) {
  if (v->_data) free(v->_data);
}

void vector_reserve(Vector* v, size_t capacity) {
  if (v->_cap < capacity) {
    v->_cap = capacity;
    v->_data = realloc(v->_data, v->_cap * v->elemsz);
  }
}
inline void vector_grow(Vector* v, size_t require) {
  if (v->_cap < require) {
    v->_cap = require * 2;
    v->_data = realloc(v->_data, v->_cap * v->elemsz);
  }
}

void* vector_push(Vector* v) {
  // Check capacity
  vector_grow(v, v->size+1);
  return v->_data + (v->size++ * v->elemsz);
}

void vector_pop(Vector* v) {
  assert(v->size > 0);
  v->size--;
}
