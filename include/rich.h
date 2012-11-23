#ifndef RICH_H_E9E4E2E787721B
#define RICH_H_E9E4E2E787721B

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/io.h>

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
  bool (*sink_nil)(void* self);
  bool (*sink_bool)(void* self, bool val);
  bool (*sink_int)(void* self, int val);
  bool (*sink_float)(void* self, double val);
  bool (*sink_string)(void* self, const char* str, size_t sz);

  bool (*begin_array)(void* self);
  bool (*end_array)(void* self);

  bool (*begin_map)(void* self);
  bool (*sink_key)(void* self, const char* str, size_t sz);
  bool (*end_map)(void* self);

  void (*close)(void* self);
};

// Reads rich data from some source and passes it to a rich_Sink
interface(rich_Source) {
  bool (*read_value)(void* self, rich_Sink* to);
  void (*close)(void* self);
};

// Encodes and decodes rich data
interface(rich_Codec) {
  rich_Sink*    (*new_sink)(void* self, Output* out);
  rich_Source*  (*new_source)(void* self, Input* in);
};

extern rich_Sink rich_debug_sink;

#endif /* RICH_H_E9E4E2E787721B */

