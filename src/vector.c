
#include <stdlib.h>
#include <string.h>
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

/* AutoVector */

void  autovector_init(AutoVector* self, ResourceManager* manager) {
  self->manager = manager;
  vector_init(self->v, call(manager, data_size), 4);
  memset(self->v->_data, 0, self->v->_cap * self->v->elemsz);
  for (unsigned i = 0; i < self->v->_cap; i++) {
    call(self->manager, reset_value, self->v->_data + i*self->v->elemsz);
  }
}
void autovector_close(AutoVector* self) {
  for (unsigned i = 0; i < self->v->_cap; i++) {
    call(self->manager, close_value, self->v->_data + i*self->v->elemsz);
  }
  vector_close(self->v);
  call(self->manager, close);
}
void autovector_reset(AutoVector* self) {
  for (unsigned i = 0; i < self->v->size; i++) {
    call(self->manager, reset_value, vector_get(self->v, i));
  }
  vector_clear(self->v);
}

void* autovector_push(AutoVector* self) {
  size_t oldcap = self->v->_cap;
  void* value = vector_push(self->v);
  if (self->v->_cap > oldcap) {
    memset(self->v->_data + oldcap*self->v->elemsz, 0, (self->v->_cap - oldcap) * self->v->elemsz);
    for (unsigned i = oldcap; i < self->v->_cap; i++) {
      call(self->manager, reset_value, self->v->_data + i*self->v->elemsz);
    }
  }
  return value;
}