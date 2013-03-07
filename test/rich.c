
#include <stdlib.h>
#include <stdio.h>

#include <vlib/test.h>
#include <vlib/rich.h>
#include <vlib/io.h>

static void sink_key(rich_Sink* sink, const char* key) {
  Bytes str = {
    .ptr = (void*)key,
    .size = strlen(key),
  };
  call(sink, sink, RICH_KEY, &str);
}
static void sink_string(rich_Sink* sink, const char* key) {
  Bytes str = {
    .ptr = (void*)key,
    .size = strlen(key),
  };
  call(sink, sink, RICH_STRING, &str);
}
static void sink_int(rich_Sink* sink, int64_t ival) {
  call(sink, sink, RICH_INT, &ival);
}

static int json_encode() {
  Output* out = string_output_new(256);
  rich_Sink* sink = call(rich_codec_json, new_sink, out);
  call(sink, sink, RICH_MAP, NULL);
    sink_key(sink, "name");
    sink_string(sink, "Vaughan\tNewton\r\n");
    sink_key(sink, "age");
    sink_int(sink, 20);
    sink_key(sink, "interests");
    call(sink, sink, RICH_ARRAY, NULL);
      sink_string(sink, "programming");
      sink_int(sink, 23);
      sink_string(sink, "cats");
    call(sink, sink, RICH_ENDARRAY, NULL);
  call(sink, sink, RICH_ENDMAP, NULL);

  const char* expect = "{\"name\":\"Vaughan\\tNewton\\r\\n\",\"age\":20,\"interests\":[\"programming\",23,\"cats\"]}";
  size_t result_size;
  const char* result = string_output_data(out, &result_size);
  assertEqual(result_size, strlen(expect));
  assertTrue(memcmp(expect, result, result_size) == 0);

  call(sink, close);
  return 0;
}

VLIB_SUITE(rich) = {
  VLIB_TEST(json_encode),
  VLIB_END
};