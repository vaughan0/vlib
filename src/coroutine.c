
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlib/coroutine.h>

data(Frame) {
  co_State* state;
  char      udata[];
};

void coroutine_init(Coroutine* self) {
  bytestack_init(self->stack, 64);
}
void coroutine_close(Coroutine* self) {
  while (self->stack->size) {
    coroutine_pop(self);
  }
  bytestack_close(self->stack);
}

void* coroutine_push(Coroutine* self, co_State* state, size_t udata_size) {
  Frame* frame = bytestack_push(self->stack, sizeof(Frame) + udata_size);
  frame->state = state;
  return frame->udata;
}
void coroutine_pop(Coroutine* self) {
  Frame* frame = bytestack_top(self->stack);
  if (frame->state->close) frame->state->close(frame->udata);
  bytestack_pop(self->stack);
}

void coroutine_run(Coroutine* self, void* arg) {
  Frame* frame = bytestack_top(self->stack);
  frame->state->run(frame->udata, self, arg);
}