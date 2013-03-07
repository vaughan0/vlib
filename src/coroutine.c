
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlib/coroutine.h>

static inline co_State* top_state(Coroutine* self) {
  size_t offset = *(size_t*)vector_back(self->stack);
  return (co_State*)(self->data + offset);
}

void coroutine_init(Coroutine* self) {
  vector_init(self->stack, sizeof(size_t), 4);
  self->data_size = 32;
  self->data = malloc(self->data_size);
  self->data_used = 0;
}
void coroutine_close(Coroutine* self) {
  while (self->stack->size) {
    coroutine_pop(self);
  }
  vector_close(self->stack);
  free(self->data);
}

void coroutine_push(Coroutine* self, co_State_Impl* impl, void* init_arg) {
  assert(impl->size >= sizeof(co_State));
  assert(impl->run);

  // Grow stack data if needed
  size_t require = self->data_used + impl->size;
  if (require > self->data_size) {
    self->data_size = require * 2;
    self->data = realloc(self->data, self->data_size);
  }

  *(size_t*)vector_push(self->stack) = self->data_used;
  self->data_used = require;
  co_State* state = top_state(self);
  state->_impl = impl;
  if (impl->init) impl->init(state, init_arg);
}
void coroutine_pop(Coroutine* self) {
  co_State* top = top_state(self);
  if (top->_impl->close) call(top, close);
  self->data_used = *(size_t*)vector_back(self->stack);
  vector_pop(self->stack);
}

void coroutine_run(Coroutine* self, void* arg) {
  co_State* state = top_state(self);
  call(state, run, self, arg);
}