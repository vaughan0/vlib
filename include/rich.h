#ifndef RICH_H_E9E4E2E787721B
#define RICH_H_E9E4E2E787721B

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/error.h>
#include <vlib/vector.h>

typedef enum {
  RICH_NIL,
  RICH_BOOL,
  RICH_INT,
  RICH_FLOAT,
  RICH_STRING,
  RICH_ARRAY,
  RICH_MAP,
} rich_Type;

// Collection of callbacks for handling rich data is it is processed.
interface(rich_Sink) {
  void (*sink_nil)(void* self);
  void (*sink_bool)(void* self, bool val);
  void (*sink_int)(void* self, int val);
  void (*sink_float)(void* self, double val);
  void (*sink_string)(void* self, const char* str, size_t sz);

  void (*begin_array)(void* self);
  void (*end_array)(void* self);

  void (*begin_map)(void* self);
  void (*sink_key)(void* self, const char* str, size_t sz);
  void (*end_map)(void* self);

  void (*close)(void* self);
};

// Reads rich data from some source and passes it to a rich_Sink
interface(rich_Source) {
  void (*read_value)(void* self, rich_Sink* to);
  void (*close)(void* self);
};

// Encodes and decodes rich data
interface(rich_Codec) {
  rich_Sink*    (*new_sink)(void* self, Output* out);
  rich_Source*  (*new_source)(void* self, Input* in);
};

extern rich_Sink rich_debug_sink;

extern rich_Codec rich_codec_json;

/* Reactor utility */

data(rich_Reactor) {
  rich_Sink   base;
  Vector      stack;
};

void  rich_reactor_init(rich_Reactor* self, size_t stack_data);
void  rich_reactor_close(rich_Reactor* self);

// Pushes a new sink on the stack and returns a pointer to the associated extra stack data
void* rich_reactor_push(rich_Reactor* self, rich_Sink* schema);
// Pops a sink from the stack
void  rich_reactor_pop(rich_Reactor* self);


#endif /* RICH_H_E9E4E2E787721B */

