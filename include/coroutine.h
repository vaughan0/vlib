#ifndef VLIB_COROUTINE_H
#define VLIB_COROUTINE_H

#include <vlib/std.h>
#include <vlib/bytestack.h>

struct Coroutine;

data(co_State) {
  void      (*run)(void* udata, struct Coroutine* co, void* arg);
  void      (*close)(void* udata);
};

data(Coroutine) {
  ByteStack stack[1];
};

void  coroutine_init(Coroutine* self);
void  coroutine_close(Coroutine* self);

// Pushes a new state onto the coroutine's stack and allocates `udata_size` bytes for
// the state's user data. Returns a pointer to the user data.
void* coroutine_push(Coroutine* self, co_State* state, size_t udata_size);
void  coroutine_pop(Coroutine* self);

void  coroutine_run(Coroutine* self, void* arg);

#endif
