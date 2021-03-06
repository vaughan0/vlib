
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>
#include <vlib/hashtable.h>
#include <vlib/util.h>

/* Debug sink */

static void print_string(Bytes* str) {
  fwrite(str->ptr, 1, str->size, stdout);
}
static void debug_sink(void* _self, rich_Atom atom, void* data) {
  switch (atom) {
    case RICH_NIL:
      printf("nil\n");
      break;
    case RICH_BOOL:
      printf(*(bool*)data ? "true\n" : "false\n");
      break;
    case RICH_INT:
      printf("%ld\n", *(int64_t*)data);
      break;
    case RICH_FLOAT:
      printf("%lf\n", *(double*)data);
      break;
    case RICH_STRING:
      printf("\""); print_string(data); printf("\"\n");
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
      print_string(data); printf(" => ");
      break;
    case RICH_ENDMAP:
      printf("}\n");
      break;
  }
}

static rich_Sink_Impl debug_sink_impl = {
  .sink = debug_sink,
  .close = null_close,
};

rich_Sink rich_debug_sink[1] = {{
  ._impl = &debug_sink_impl,
}};
