#ifndef RICH_BIND_H_9AA13EB5361B53
#define RICH_BIND_H_9AA13EB5361B53

#include <vlib/rich.h>
#include <vlib/coroutine.h>

interface(rich_Schema) {
  void    (*dump_value)(void* self, void* value, rich_Sink* to);
};

#endif /* RICH_BIND_H_9AA13EB5361B53 */
