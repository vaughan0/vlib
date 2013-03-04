#ifndef RICH_BIND_H_D656D10B3990D3
#define RICH_BIND_H_D656D10B3990D3

#include <vlib/rich.h>

interface(rich_Schema) {
  rich_Reactor_Sink_Impl sink_impl;
  void    (*dump_value)(void* self, void* from, rich_Sink* to);
  size_t  (*data_size)(void* self);
  void    (*close_data)(void* self, void* data);
  void    (*close)(void* self);
};

// The reactor's data field will point to one of these
data(rich_Frame) {
  void*   to;     // pointer to bound data
  void*   udata;  // custom per-schema data
};

static inline void rich_schema_close(rich_Schema* s) {
  if (s->_impl->close) call(s, close);
}

/* Creates a new rich_Source from a schema and address.
 * If you want to change the address, it's probably easiest to wrap the schema in a pointer schema,
 * then just reassign the pointer to rebind. */
rich_Source*  rich_schema_source(rich_Schema* schema, void* from);

// Dumps data of a certain schema, from a given address, to a sink
void rich_dump(rich_Schema* schema, void* from, rich_Sink* to);

/* Initializes a bind operation with a given schema and destination address.
 * Returns a sink which, when written to, will fill in the address with structured
 * data according to the schema.
 */
rich_Sink*  rich_bind(rich_Schema* schema, void* to);
void        rich_rebind(rich_Sink* bound_sink, void* to);

// Utility for recursive schema implementations
static inline void rich_schema_push(rich_Reactor* r, rich_Schema* s, void* to) {
  rich_Frame* f = rich_reactor_push(r, (rich_Reactor_Sink*)s);
  f->to = to;
}

/* Default schemas */

extern rich_Schema rich_schema_bool;
extern rich_Schema rich_schema_int;
extern rich_Schema rich_schema_float;
extern rich_Schema rich_schema_string;  // rich_String
extern rich_Schema rich_schema_cstring; // NULL-terminated strings

// Does nothing with the data it receives
extern rich_Schema rich_schema_discard;

rich_Schema*  rich_schema_vector(rich_Schema* vector_of);

// Uses rich_String structs as keys
rich_Schema*  rich_schema_hashtable(rich_Schema* hashtable_of);

/** Struct schema:
 *
 * A struct schema fills in data from a rich map.
 * It has a set of registered sub-schemas, each with a name (corresponding to a map key),
 * and an offset, which will point to the data inside the target struct.
 */

rich_Schema*  rich_schema_struct(size_t struct_size);
void          rich_struct_set_ignore_unknown(void* struct_schema, bool ignore);
void          rich_struct_register(void* struct_schema, rich_String* name, size_t offset, rich_Schema* schema);
void          rich_struct_cregister(void* struct_schema, const char* name, size_t offset, rich_Schema* schema);

/** Tuple schema:
 *
 * Works similarly to a struct schema, but uses ordered elements in an array instead
 * of named map elements.
 */

rich_Schema*  rich_schema_tuple(size_t struct_size);
void          rich_tuple_add(void* tuple_schema, size_t offset, rich_Schema* schema);

// Allocates data and delegates it to another schema
rich_Schema*  rich_schema_pointer(rich_Schema* sub);

#endif /* RICH_BIND_H_D656D10B3990D3 */

