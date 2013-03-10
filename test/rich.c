
#include <stdlib.h>
#include <stdio.h>

#include <vlib/test.h>
#include <vlib/io.h>
#include <vlib/rich.h>
#include <vlib/rich_schema.h>

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

data(Person) {
  Bytes   name;
  int64_t age;
  bool    is_criminal;
  Vector  interests[1];
};
static rich_Schema* person_schema() {
  rich_Schema* s = rich_schema_struct(sizeof(Person));
  RICH_ADD_FIELD(s, Person, name, rich_schema_bytes);
  RICH_ADD_FIELD(s, Person, age, rich_schema_int64);
  RICH_ADD_FIELD(s, Person, is_criminal, rich_schema_bool);
  rich_Schema* interests = rich_schema_vector(rich_schema_bytes);
  RICH_ADD_FIELD(s, Person, interests, interests);
  return s;
}

static int struct_schema_encode() {
  rich_Schema* schema = person_schema();

  Person p = {};
  bytes_ccopy(&p.name, "vaughan");
  p.age = 20;
  p.is_criminal = false;
  rich_Source* source = rich_bind_source(schema, &p);

  Output* out = string_output_new(256);
  rich_Sink* sink = call(rich_codec_json, new_sink, out);
  call(source, read_value, sink);

  const char* expect = "{\"name\":\"vaughan\",\"age\":20,\"is_criminal\":false,\"interests\":null}";
  size_t expectsz = strlen(expect);
  size_t resultsz;
  const char* result = string_output_data(out, &resultsz);
  assertEqual(resultsz, expectsz);
  assertTrue(memcmp(expect, result, resultsz) == 0);

  bytes_close(&p.name);
  call(sink, close);
  call(source, close);
  return 0;
}
static int struct_schema_decode() {
  rich_Schema* schema = person_schema();

  const char* json = "{\"age\":8,\"name\":\"seth\",\"is_criminal\":true,\"interests\":[\"mice\",\"treason\"]}";
  Input* in = memory_input_new(json, strlen(json));
  rich_Source* source = call(rich_codec_json, new_source, in);

  Person p = {};
  rich_Sink* sink = rich_bind_sink(schema, &p);
  call(source, read_value, sink);

  Bytes name = {};
  bytes_ccopy(&name, "seth");
  assertTrue(bytes_compare(&name, &p.name) == 0);
  assertEqual(p.age, 8);
  assertEqual(p.is_criminal, true);

  assertEqual(p.interests->size, 2);
  Bytes interest = {};
  bytes_ccopy(&interest, "mice");
  assertTrue(bytes_compare(&interest, vector_get(p.interests, 0)) == 0);
  bytes_ccopy(&interest, "treason");
  assertTrue(bytes_compare(&interest, vector_get(p.interests, 1)) == 0);

  call(source, close);
  call(sink, close);
  bytes_close(&name);
  bytes_close(&interest);
  return 0;
}

VLIB_SUITE(rich) = {
  VLIB_TEST(json_encode),
  VLIB_TEST(struct_schema_encode),
  VLIB_TEST(struct_schema_decode),
  VLIB_END
};