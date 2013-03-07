#ifndef RICH_BIND_H_9AA13EB5361B53
#define RICH_BIND_H_9AA13EB5361B53

#include <vlib/rich.h>
#include <vlib/coroutine.h>

interface(rich_Schema) {
  void    (*dump_value)(void* self, void* value, rich_Sink* to);
  void    (*reset_value)(void* self, void* value);
  void    (*close_value)(void* self, void* value);
  void    (*push_state)(void* self, Coroutine* co);
  void    (*close)(void* self);
};

data(rich_SchemaArg) {
  void*     value;
  rich_Atom atom;
  void*     data;
};

rich_Source*  rich_bind_source(rich_Schema* schema, void* from);
rich_Sink*    rich_bind_sink(rich_Schema* schema, void* to);

extern rich_Schema rich_schema_bool[1];

#endif /* RICH_BIND_H_9AA13EB5361B53 */
