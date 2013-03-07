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
  // name         data type
  RICH_NIL,       // no data
  RICH_BOOL,      // C bool type
  RICH_INT,       // C int64 type
  RICH_FLOAT,     // C double type
  RICH_STRING,    // rich_String
  RICH_ARRAY,     // no data
  RICH_ENDARRAY,  // no data
  RICH_MAP,       // no data
  RICH_KEY,       // rich_String
  RICH_ENDMAP,    // no data
} rich_Atom;

data(rich_String) {
  size_t  sz;
  char*   data;
};

bool rich_string_equal(const rich_String* a, const rich_String* b);

// rich_String hashtable key support
uint64_t rich_string_hasher(const void* k, size_t sz);
int rich_string_equaler(const void* a, const void* b, size_t sz);

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

/* Reactor utility for creating rich_Sinks */

data(rich_Reactor) {
  rich_Sink   base;
  void*       frame;
  void*       global;
  // closer will be called when the reactor is closed, if it is not NULL
  void        (*closer)(rich_Reactor* reactor);
  /* Private fields */
  Vector      stack[1];
  char*       stack_data;
  size_t      stack_cap;
  char        global_data[];
};

data(rich_ReactorSink) {
  size_t    data_size;
  void      (*close_frame)(rich_Reactor* reactor);
  void      (*sink)(rich_Reactor* reactor, rich_Atom atom, void* atom_data);
};

// Creates a new rich_Reactor and allocates `global_data_size` bytes for the reactor's `global` field.
rich_Reactor* rich_reactor_new(size_t global_data_size);
void    rich_reactor_reset(rich_Reactor* self);

// Pushes a new sink onto the reactor and returns a pointer to the sink's frame data.
void*   rich_reactor_push(rich_Reactor* self, const rich_ReactorSink* sink);
// Pops the top sink off the reactor.
void    rich_reactor_pop(rich_Reactor* self);

void    rich_reactor_sink(rich_Reactor* self, rich_Atom atom, void* atom_data);

#endif /* RICH_H_E9E4E2E787721B */
