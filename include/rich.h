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

extern rich_Codec rich_json_codec[1];

/** Reactor utility
 *
 * Note: when pushing and popping from/to the reactor, the current frame data
 * (r->data) may be moved, so be sure to either save the data somewhere or get
 * the pointer from the reactor again after pushing/popping.
 */

data(rich_Reactor) {
  rich_Sink   base;
  bool        pop_last; // If false, the last sink will be left on the stack instead of being popped
  Vector      stack[1];
  void*       data;
  void*       global;   // global across stack frames, for user access
};

interface(rich_Reactor_Sink) {
  // Called just after the sink is pushed onto the stack, but before any data is passed to it
  void  (*init_frame)(void* self, void* data);
  // Called just after the sink is popped from the stack
  void  (*cleanup_frame)(void* self, void* data);
  void  (*sink)(void* self, rich_Reactor* r, rich_Atom atom, void* atom_data);
};

void  rich_reactor_init(rich_Reactor* self, size_t stack_data_sz);
void  rich_reactor_close(rich_Reactor* self);
void  rich_reactor_reset(rich_Reactor* self);

// Pushes a new sink on the stack and returns a pointer to the associated extra stack data
void* rich_reactor_push(rich_Reactor* self, rich_Reactor_Sink* sink);
void  rich_reactor_pop(rich_Reactor* self);

#endif /* RICH_H_E9E4E2E787721B */

