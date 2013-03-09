#ifndef RICH_BIND_H_9AA13EB5361B53
#define RICH_BIND_H_9AA13EB5361B53

#include <vlib/rich.h>
#include <vlib/coroutine.h>

interface(rich_Schema) {
  size_t  (*data_size)(void* self);
  void    (*dump_value)(void* self, void* value, rich_Sink* to);
  void    (*reset_value)(void* self, void* value);
  void    (*close_value)(void* self, void* value);
  void    (*push_state)(void* self, Coroutine* co, void* value);
  void    (*close)(void* self);
};

rich_Schema*        rich_schema_unclosable(rich_Schema* wrap);
void                rich_schema_close(rich_Schema* unclosable);

data(rich_SchemaArg) {
  rich_Atom atom;
  void*     data;
};

rich_Source*  rich_bind_source(rich_Schema* schema, void* from);
rich_Sink*    rich_bind_sink(rich_Schema* schema, void* to);

extern rich_Schema  rich_schema_bool[1];
extern rich_Schema  rich_schema_int64[1];
extern rich_Schema  rich_schema_double[1];
extern rich_Schema  rich_schema_bytes[1];

rich_Schema*        rich_schema_pointer(rich_Schema* to);
rich_Schema*        rich_schema_optional(rich_Schema* wrap);

rich_Schema*        rich_schema_vector(rich_Schema* of);

// Uses Bytes objects as keys.
rich_Schema*        rich_schema_hashtable(rich_Schema* of);

rich_Schema*        rich_schema_struct();
void                rich_add_field(rich_Schema* self, const char* name, rich_Schema* field_type);

#endif /* RICH_BIND_H_9AA13EB5361B53 */
