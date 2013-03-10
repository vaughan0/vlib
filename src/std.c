
#include <stdlib.h>
#include <string.h>

#include <vlib/std.h>
#include <vlib/util.h>

void bytes_init(Bytes* self, size_t cap) {
  self->size = 0;
  self->cap = cap;
  self->ptr = malloc(cap);
}
void bytes_close(Bytes* self) {
  if (self->ptr) {
    free(self->ptr);
    self->ptr = NULL;
  }
}
void bytes_grow(Bytes* self, size_t require) {
  if (!self->ptr) {
    self->cap = require;
    self->ptr = malloc(require);
  } else if (require > self->cap) {
    self->cap = require * 2;
    self->ptr = realloc(self->ptr, self->cap);
  }
}

void bytes_copy(Bytes* self, const Bytes* from) {
  bytes_grow(self, from->size);
  self->size = from->size;
  memcpy(self->ptr, from->ptr, self->size);
}
void bytes_ccopy(Bytes* self, const char* str) {
  Bytes from = {
    .ptr = (void*)str,
    .size = strlen(str),
  };
  bytes_copy(self, &from);
}

int bytes_compare(const Bytes* a, const Bytes* b) {
  unsigned char* ap = a->ptr;
  unsigned char* bp = b->ptr;
  unsigned stop = MIN(a->size, b->size);
  for (unsigned i = 0; i < stop; i++) {
    if (ap[i] < bp[i]) return -1;
    if (ap[i] > bp[i]) return 1;
  }
  if (a->size < b->size) return -1;
  if (a->size > b->size) return 1;
  return 0; 
}