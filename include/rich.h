#ifndef RICH_H_E9E4E2E787721B
#define RICH_H_E9E4E2E787721B

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/error.h>
#include <vlib/coroutine.h>

typedef enum {
  // name         data type
  RICH_NIL,       // no data
  RICH_BOOL,      // C bool type
  RICH_INT,       // C int64 type
  RICH_FLOAT,     // C double type
  RICH_STRING,    // Bytes
  RICH_ARRAY,     // no data
  RICH_ENDARRAY,  // no data
  RICH_MAP,       // no data
  RICH_KEY,       // Bytes
  RICH_ENDMAP,    // no data
} rich_Atom;

// Handles incoming rich data
interface(rich_Sink) {
  void (*sink)(void* self, rich_Atom atom, void* atom_data);
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
  void          (*close)(void* self);
};

extern rich_Sink rich_debug_sink[1];

extern rich_Codec rich_codec_json[1];

/* Using coroutines to implement sinks */

data(rich_CoSink) {
  rich_Sink   base;
  Coroutine   co[1];
  void        (*closer)(void* shared_data);
  char        shared_data[];
};

rich_CoSink*  rich_cosink_new(size_t shared_data_size);

static inline void* rich_cosink_shared(rich_CoSink* self) {
  return self->shared_data;
}

data(rich_SinkArg) {
  rich_Atom   atom;
  void*       data;
  void*       shared;
};

#endif /* RICH_H_E9E4E2E787721B */
