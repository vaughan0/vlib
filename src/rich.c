
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>

/* Debug sink */

static void debug_sink(void* _self, rich_Atom atom, void* data) {
  switch (atom) {
    case RICH_NIL:
      printf("nil\n");
      break;
    case RICH_BOOL:
      printf(*(bool*)data ? "true\n" : "false\n");
      break;
    case RICH_INT:
      printf("%d\n", *(int*)data);
      break;
    case RICH_FLOAT:
      printf("%lf\n", *(double*)data);
      break;
    case RICH_STRING:
      printf("\"%s\"\n", ((rich_String*)data)->data);
      break;
    case RICH_ARRAY:
      printf("[\n");
      break;
    case RICH_ENDARRAY:
      printf("]\n");
      break;
    case RICH_MAP:
      printf("{\n");
      break;
    case RICH_KEY:
      printf("%s => ", ((rich_String*)data)->data);
      break;
    case RICH_ENDMAP:
      printf("}\n");
      break;
  }
}

static void debug_close(void* _self) {}

static rich_Sink_Impl debug_sink_impl = {
  .sink = debug_sink,
  .close = debug_close,
};

rich_Sink rich_debug_sink = {
  ._impl = &debug_sink_impl,
};

/* Reactor */

