
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/bytestack.h>

static inline size_t align(size_t sz) {
  return ((sz+sizeof(void*)-1)/sizeof(void*)) * sizeof(void*);
}

void bytestack_init(ByteStack* self, size_t init_cap) {
  self->cap = align(init_cap);
  self->size = 0;
  self->data = malloc(init_cap);
  vector_init(self->stack, sizeof(size_t), 4);
}
void bytestack_close(ByteStack* self) {
  free(self->data);
  vector_close(self->stack);
}

void* bytestack_push(ByteStack* self, size_t size) {
  size = align(size);
  size_t require = self->size + size;
  if (require > self->cap) {
    self->cap = require * 2;
    self->data = realloc(self->data, self->cap);
  }
  void* data = self->data + self->size;
  *(size_t*)vector_push(self->stack) = self->size;
  self->size = require;
  return data;
}

void* bytestack_top(ByteStack* self) {
  size_t offset = *(size_t*)vector_back(self->stack);
  return self->data + offset;
}

void bytestack_pop(ByteStack* self) {
  self->size = *(size_t*)vector_back(self->stack);
  vector_pop(self->stack);
}
