#ifndef VLIB_COROUTINE_H
#define VLIB_COROUTINE_H

#include <vlib/std.h>
#include <vlib/vector.h>

struct Coroutine;

interface(co_State) {
  size_t  size;
  void    (*init)(void* self, void* init_arg);
  void    (*run)(void* self, struct Coroutine* co, void* arg);
  void    (*close)(void* self);
};

data(Coroutine) {
  Vector    stack[1];
  char*     data;
  size_t    data_size;
  size_t    data_used;
};

void  coroutine_init(Coroutine* self);
void  coroutine_close(Coroutine* self);

void  coroutine_push(Coroutine* self, co_State_Impl* impl, void* init_arg);
void  coroutine_pop(Coroutine* self);

void  coroutine_run(Coroutine* self, void* arg);

#endif
