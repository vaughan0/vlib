
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/rich.h>

/* Debug sink */

static void debug_sink_nil(void* _self) {
  printf("nil\n");
}
static void debug_sink_bool(void* _self, bool val) {
  printf("%s\n", val ? "true" : "false");
}
static void debug_sink_int(void* _self, int val) {
  printf("%d\n", val);
}
static void debug_sink_float(void* _self, double val) {
  printf("%lf\n", val);
}
static void debug_sink_string(void* _self, const char* str, size_t sz) {
  printf("\"%s\"\n", str);
}

static void debug_begin_array(void* _self) {
  printf("[\n");
}
static void debug_end_array(void* _self) {
  printf("]\n");
}

static void debug_begin_map(void* _self) {
  printf("{\n");
}
static void debug_sink_key(void* _self, const char* str, size_t sz) {
  printf("\"%s\" => ", str);
}
static void debug_end_map(void* _self) {
  printf("}\n");
}

static void debug_close(void* _self) {}

static rich_Sink_Impl debug_sink_impl = {
  .sink_nil = debug_sink_nil,
  .sink_bool = debug_sink_bool,
  .sink_int = debug_sink_int,
  .sink_float = debug_sink_float,
  .sink_string = debug_sink_string,
  .begin_array = debug_begin_array,
  .end_array = debug_end_array,
  .begin_map = debug_begin_map,
  .sink_key = debug_sink_key,
  .end_map = debug_end_map,
  .close = debug_close,
};

rich_Sink rich_debug_sink = {
  ._impl = &debug_sink_impl,
};

/* Reactor */


