
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>

bool rich_string_equal(const rich_String* a, const rich_String* b) {
  if (a->sz != b->sz) return false;
  return memcmp(a->data, b->data, a->sz) == 0;
}

/* Debug sink */

static void debug_sink(void* _self, rich_Atom atom, void* data) {
  switch (atom) {
    case RICH_NIL:
      printf("nil\n");
      break;
    case RICH_BOOL:
      printf(*(bool*)data ? "true\n" : "false\n");
      break;
    case RICH_INT:
      printf("%d\n", *(int*)data);
      break;
    case RICH_FLOAT:
      printf("%lf\n", *(double*)data);
      break;
    case RICH_STRING:
      printf("\"%s\"\n", ((rich_String*)data)->data);
      break;
    case RICH_ARRAY:
      printf("[\n");
      break;
    case RICH_ENDARRAY:
      printf("]\n");
      break;
    case RICH_MAP:
      printf("{\n");
      break;
    case RICH_KEY:
      printf("%s => ", ((rich_String*)data)->data);
      break;
    case RICH_ENDMAP:
      printf("}\n");
      break;
  }
}

static void debug_close(void* _self) {}

static rich_Sink_Impl debug_sink_impl = {
  .sink = debug_sink,
  .close = debug_close,
};

rich_Sink rich_debug_sink = {
  ._impl = &debug_sink_impl,
};

/* Reactor */

static rich_Sink_Impl reactor_impl;

data(Frame) {
  rich_Reactor_Sink* sink;
  char data[];
};

void rich_reactor_init(rich_Reactor* self, size_t data_sz) {
  self->base._impl = &reactor_impl;
  vector_init(self->stack, sizeof(Frame) + data_sz, 8);
  self->data = NULL;
}
void rich_reactor_close(rich_Reactor* self) {
  vector_close(self->stack);
}
void rich_reactor_reset(rich_Reactor* self) {
  vector_clear(self->stack);
}

void* rich_reactor_push(rich_Reactor* self, rich_Reactor_Sink* sink) {
  Frame* f = vector_push(self->stack);
  f->sink = sink;
  self->data = f->data;
  return f->data;
}
void rich_reactor_pop(rich_Reactor* self) {
  vector_pop(self->stack);
  if (self->stack->size){
    Frame* f = vector_back(self->stack);
    self->data = f->data;
  } else {
    self->data = NULL;
  }
}

static void reactor_sink(void* _self, rich_Atom atom, void* data) {
  rich_Reactor* self = _self;
  Frame* f = vector_back(self->stack);
  call(f->sink, sink, self, atom, data);
}
static void reactor_close(void* _self) {}

static rich_Sink_Impl reactor_impl = {
  .sink = reactor_sink,
  .close = reactor_close,
};
