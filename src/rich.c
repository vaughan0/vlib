
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>
#include <vlib/hashtable.h>
#include <vlib/util.h>

bool rich_string_equal(const rich_String* a, const rich_String* b) {
  if (a->sz != b->sz) return false;
  return memcmp(a->data, b->data, a->sz) == 0;
}

uint64_t rich_string_hasher(const void* k, size_t sz) {
  const rich_String* str = k;
  return hasher_fnv64(str->data, str->sz);
}
int rich_string_equaler(const void* _a, const void* _b, size_t sz) {
  const rich_String *a = _a, *b = _b;
  if (a->sz != b->sz) return -1;
  return memcmp(a->data, b->data, a->sz);
}

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

static rich_Sink_Impl debug_sink_impl = {
  .sink = debug_sink,
  .close = null_close,
};

rich_Sink rich_debug_sink[1] = {{
  ._impl = &debug_sink_impl,
}};
