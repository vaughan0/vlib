
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>

/* Debug sink */

static void debug_sink_nil(void* _self) {
  printf("nil\n");
}
static void debug_sink_bool(void* _self, bool val) {
  printf("%s\n", val ? "true" : "false");
}
static void debug_sink_int(void* _self, int val) {
  printf("%d\n", val);
}
static void debug_sink_float(void* _self, double val) {
  printf("%lf\n", val);
}
static void debug_sink_string(void* _self, const char* str, size_t sz) {
  printf("\"%s\"\n", str);
}

static void debug_begin_array(void* _self) {
  printf("[\n");
}
static void debug_end_array(void* _self) {
  printf("]\n");
}

static void debug_begin_map(void* _self) {
  printf("{\n");
}
static void debug_sink_key(void* _self, const char* str, size_t sz) {
  printf("\"%s\" => ", str);
}
static void debug_end_map(void* _self) {
  printf("}\n");
}

static void debug_close(void* _self) {}

static rich_Sink_Impl debug_sink_impl = {
  .sink_nil = debug_sink_nil,
  .sink_bool = debug_sink_bool,
  .sink_int = debug_sink_int,
  .sink_float = debug_sink_float,
  .sink_string = debug_sink_string,
  .begin_array = debug_begin_array,
  .end_array = debug_end_array,
  .begin_map = debug_begin_map,
  .sink_key = debug_sink_key,
  .end_map = debug_end_map,
  .close = debug_close,
};

rich_Sink rich_debug_sink = {
  ._impl = &debug_sink_impl,
};

/* Reactor */

data(Frame) {
  rich_Reactor_Sink* sink;
  char data[];
};

#define FORWARD(method, ...) \
  rich_Reactor* self = _self; \
  Frame* frame = vector_back(&self->stack); \
  if (frame->sink->_impl->method) { \
    call(frame->sink, method, self, ##__VA_ARGS__); \
  } else { \
    verr_raise(VERR_MALFORMED); \
  }

static void reactor_sink_nil(void* _self) { FORWARD(sink_nil); }
static void reactor_sink_bool(void* _self, bool val) { FORWARD(sink_bool, val); }
static void reactor_sink_int(void* _self, int val) { FORWARD(sink_int, val); }
static void reactor_sink_float(void* _self, double val) { FORWARD(sink_float, val); }
static void reactor_sink_string(void* _self, const char* str, size_t sz) { FORWARD(sink_string, str, sz); }
static void reactor_begin_array(void* _self) { FORWARD(begin_array); }
static void reactor_end_array(void* _self) { FORWARD(end_array); }
static void reactor_begin_map(void* _self) { FORWARD(begin_map); }
static void reactor_sink_key(void* _self, const char* str, size_t sz) { FORWARD(sink_key, str, sz); }
static void reactor_end_map(void* _self) { FORWARD(end_map); }

static void reactor_close(void* _self) {}

static rich_Sink_Impl reactor_impl = {
  .sink_nil = reactor_sink_nil,
  .sink_bool = reactor_sink_bool,
  .sink_int = reactor_sink_int,
  .sink_float = reactor_sink_float,
  .sink_string = reactor_sink_string,
  .begin_array = reactor_begin_array,
  .end_array = reactor_end_array,
  .begin_map = reactor_begin_map,
  .sink_key = reactor_sink_key,
  .end_map = reactor_end_map,
  .close = reactor_close,
};

void rich_reactor_init(rich_Reactor* self, size_t stack_data_sz) {
  self->base._impl = &reactor_impl;
  vector_init(&self->stack, sizeof(Frame) + stack_data_sz, 8);
}
void rich_reactor_close(rich_Reactor* self) {
  vector_close(&self->stack);
}

void* rich_reactor_push(rich_Reactor* self, rich_Reactor_Sink* sink) {
  Frame* f = vector_push(&self->stack);
  f->sink = sink;
  self->data = f->data;
  return f->data;
}
void rich_reactor_pop(rich_Reactor* self) {
  vector_pop(&self->stack);
  self->data = self->stack.size ? ((Frame*)vector_back(&self->stack))->data : NULL;
}

