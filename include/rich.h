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
  RICH_INT,       // C int type
  RICH_FLOAT,     // C double type
  RICH_STRING,    // rich_String
  RICH_ARRAY,     // no data
  RICH_ENDARRAY,  // no data
  RICH_MAP,       // no data
  RICH_KEY,       // rich_String
  RICH_ENDMAP,    // no data
} rich_Atom;

data(rich_String) {
  size_t      sz;
  const char* data;
};

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
};

extern rich_Sink rich_debug_sink;

extern rich_Codec rich_codec_json;

/** Reactor utility
 *
 * Note: when pushing and popping from/to the reactor, the current frame data
 * (r->data) may be moved, so be sure to either save the data somewhere or get
 * the pointer from the reactor again after pushing/popping.
 */

data(rich_Reactor) {
  rich_Sink   base;
  Vector      stack;
  void*       data;
};

interface(rich_Reactor_Sink) {
  void  (*sink)(void* self, rich_Reactor* r, rich_Atom atom, void* atom_data);
};

void  rich_reactor_init(rich_Reactor* self, size_t stack_data_sz);
void  rich_reactor_close(rich_Reactor* self);

// Pushes a new sink on the stack and returns a pointer to the associated extra stack data
void* rich_reactor_push(rich_Reactor* self, rich_Reactor_Sink* sink);
void  rich_reactor_pop(rich_Reactor* self);

#endif /* RICH_H_E9E4E2E787721B */

