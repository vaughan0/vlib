#ifndef RICH_H_E9E4E2E787721B
#define RICH_H_E9E4E2E787721B

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/error.h>

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
  error_t (*sink_nil)(void* self);
  error_t (*sink_bool)(void* self, bool val);
  error_t (*sink_int)(void* self, int val);
  error_t (*sink_float)(void* self, double val);
  error_t (*sink_string)(void* self, const char* str, size_t sz);

  error_t (*begin_array)(void* self);
  error_t (*end_array)(void* self);

  error_t (*begin_map)(void* self);
  error_t (*sink_key)(void* self, const char* str, size_t sz);
  error_t (*end_map)(void* self);

  void (*close)(void* self);
};

// Reads rich data from some source and passes it to a rich_Sink
interface(rich_Source) {
  error_t (*read_value)(void* self, rich_Sink* to);
  void (*close)(void* self);
};

// Encodes and decodes rich data
interface(rich_Codec) {
  rich_Sink*    (*new_sink)(void* self, Output* out);
  rich_Source*  (*new_source)(void* self, Input* in);
};

extern rich_Sink rich_debug_sink;

extern rich_Codec rich_codec_json;

#endif /* RICH_H_E9E4E2E787721B */

