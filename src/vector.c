
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
  v->_data = v_malloc(elemsz * cap);
}
void vector_close(Vector* v) {
  if (v->_data) free(v->_data);
}

void* vector_push(Vector* v) {
  // Check capacity
  if (v->_cap == v->size) {
    v->_cap *= 2;
    v->_data = v_realloc(v->_data, (v->_cap * v->elemsz));
  }
  return v->_data + (v->size++ * v->elemsz);
}

void vector_pop(Vector* v) {
  assert(v->size > 0);
  v->size--;
}
