
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>
#include <vlib/hashtable.h>
#include <vlib/util.h>

bool rich_string_equal(const rich_String* a, const rich_String* b) {
  if (a->sz != b->sz) return false;
  return memcmp(a->data, b->data, a->sz) == 0;
}

uint64_t rich_string_hasher(const void* k, size_t sz) {
  const rich_String* str = k;
  return hasher_fnv64(str->data, str->sz);
}
int rich_string_equaler(const void* _a, const void* _b, size_t sz) {
  const rich_String *a = _a, *b = _b;
  if (a->sz != b->sz) return -1;
  return memcmp(a->data, b->data, a->sz);
}

/* Debug sink */

static void debug_string(rich_String* str) {
  char* copy = malloc(str->sz+1);
  memcpy(copy, str->data, str->sz);
  copy[str->sz] = 0;
  printf("\"%s\"\n", copy);
  free(copy);
}
static void debug_sink(void* _self, rich_Atom atom, void* data) {
  switch (atom) {
    case RICH_NIL:
      printf("nil\n");
      break;
    case RICH_BOOL:
      printf(*(bool*)data ? "true\n" : "false\n");
      break;
    case RICH_INT:
      printf("%ld\n", *(int64_t*)data);
      break;
    case RICH_FLOAT:
      printf("%lf\n", *(double*)data);
      break;
    case RICH_STRING:
      debug_string(data);
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

static rich_Sink_Impl debug_sink_impl = {
  .sink = debug_sink,
  .close = null_close,
};

rich_Sink rich_debug_sink[1] = {{
  ._impl = &debug_sink_impl,
}};

/* rich_Reactor */

data(Frame) {
  const rich_ReactorSink* sink;
  size_t data_offset;
};

static rich_Sink_Impl reactor_impl;

rich_Reactor* rich_reactor_new(size_t global_size) {
  rich_Reactor* self = malloc(sizeof(rich_Reactor) + global_size);
  self->base._impl = &reactor_impl;
  self->frame = NULL;
  self->global = self->global_data;
  vector_init(self->stack, sizeof(Frame), 4);
  self->stack_cap = 32;
  self->stack_data = malloc(self->stack_cap);
  return self;
}

void rich_reactor_reset(rich_Reactor* self) {
  while (self->stack->size) {
    rich_reactor_pop(self);
  }
}

static inline size_t stack_offset(rich_Reactor* self) {
  return self->stack->size ? ((Frame*)vector_back(self->stack))->data_offset : 0;
}
static size_t stack_alloc(rich_Reactor* self, size_t size) {
  size_t offset = stack_offset(self) + size;
  if (offset > self->stack_cap) {
    self->stack_cap = offset * 2;
    self->stack_data = realloc(self->stack_data, self->stack_cap);
  }
  memset(self->stack_data + offset, 0, size);
  return offset;
}
static inline void* stack_data(rich_Reactor* self, Frame* frame) {
  return self->stack_data + frame->data_offset;
}

void* rich_reactor_push(rich_Reactor* self, const rich_ReactorSink* sink) {
  size_t data_offset = stack_alloc(self, sink->data_size);
  Frame* frame = vector_push(self->stack);
  frame->sink = sink;
  frame->data_offset = data_offset;
  return stack_data(self, frame);
}
void rich_reactor_pop(rich_Reactor* self) {
  Frame* frame = vector_back(self->stack);
  self->frame = stack_data(self, frame);
  if (frame->sink->close_frame) frame->sink->close_frame(self);
  vector_pop(self->stack);
}

void rich_reactor_sink(rich_Reactor* self, rich_Atom atom, void* atom_data) {
  Frame* frame = vector_back(self->stack);
  self->frame = stack_data(self, frame);
  frame->sink->sink(self, atom, atom_data);
}

static void reactor_sink(void* _self, rich_Atom atom, void* atom_data) {
  rich_Reactor* self = _self;
  rich_reactor_sink(self, atom, atom_data);
}
static void reactor_close(void* _self) {
  rich_Reactor* self = _self;
  rich_reactor_reset(self);
  vector_close(self->stack);
  free(self->stack_data);
  if (self->closer) self->closer(self);
  free(self);
}
static rich_Sink_Impl reactor_impl = {
  .sink = reactor_sink,
  .close = reactor_close,
};
