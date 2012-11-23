#ifndef RICH_INMEM_H_CADAE66D0D2A6E
#define RICH_INMEM_H_CADAE66D0D2A6E

#include <vlib/rich.h>
#include <vlib/vector.h>
#include <vlib/hashtable.h>
#include <vlib/llist.h>

/* Data types */

data(rich_Value) {
  rich_Type type;
};
rich_Value*   rich_inmem_nil();

data(rich_Bool) {
  rich_Type   type;
  bool        value;
};
rich_Bool*    rich_inmem_bool(bool b);

data(rich_Int) {
  rich_Type   type;
  int         value;
};
rich_Int*     rich_inmem_int(int i);

data(rich_Float) {
  rich_Type   type;
  double      value;
};
rich_Float*   rich_inmem_float(double d);

data(rich_String) {
  rich_Type   type;
  size_t      size;
  char        value[0];
};
rich_String*  rich_inmem_string(const char* src, size_t sz);

data(rich_Array) {
  rich_Type   type;
  Vector      value;
};
rich_Array*   rich_inmem_array();

data(rich_Map) {
  rich_Type   type;
  Hashtable   value;
};
rich_Map*     rich_inmem_map();
void          rich_inmem_map_add(rich_Map* map, const char* key, size_t keysz, void* value);

data(rich_Key) {
  size_t      size;
  char        data[0];
};

void  rich_inmem_free(void* value);

/** Sink/Source
 *
 * The rich_InMem structure implements both rich_Sink and rich_Source. One can simply take
 * the pointer to it's sink or source field to access the appropriate functionality. rich_InMem
 * behaves like a queue, values that are written to the sink will become available at the source.
 */

data(rich_InMem) {
  rich_Sink     sink;
  rich_Source   source;
  LList         _values;
  bool          _closed;
  Vector        _states;
  rich_Key*     _key;
};

// Creates a new in-memory sink/source. To free the structure, close both the sink and source.
rich_InMem*  rich_inmem_new();

// Pops a rich value off the queue. The value must be freed with rich_inmem_free().
void* rich_inmem_pop(rich_InMem* self);

// Pushes a rich value to the queue. The queue will take ownership of the value, so the caller
// must not call rich_inmem_free() on the value.
void  rich_inmem_push(rich_InMem* self, void* value);

#endif /* RICH_INMEM_H_CADAE66D0D2A6E */

