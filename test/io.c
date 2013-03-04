
#include <string.h>
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/io.h>

static void write_cstr(Output* out, const char* str) {
  unsigned n = strlen(str);
  io_write(out, str, n);
}

static int string_io_write() {
  Output* out = string_output_new(1);

  write_cstr(out, "Hello ");
  write_cstr(out, "World!");
  write_cstr(out, "\nFOOBAR");

  const char* expect = "Hello World!\nFOOBAR";
  size_t sz;
  const char* result = string_output_data(out, &sz);
  assertEqual(strlen(expect), sz);
  assertEqual(strcmp(result, expect), 0);

  call(out, close);
  return 0;
};
static int string_io_put() {
  Output* out = string_output_new(1);
  io_put(out, 't');
  io_put(out, 'e');
  io_put(out, 's');
  io_put(out, 't');
  size_t sz;
  const char* result = string_output_data(out, &sz);
  assertEqual(sz, 4);
  assertEqual(strcmp(result, "test"), 0);
  call(out, close);
  return 0;
}
static int string_io_reset() {
  Output* out = string_output_new(1);

  write_cstr(out, "Hello ");
  write_cstr(out, "World!");
  string_output_reset(out);
  write_cstr(out, "Correct");
  write_cstr(out, " Result");

  const char* expect = "Correct Result";
  size_t sz;
  const char* result = string_output_data(out, &sz);
  assertEqual(strlen(expect), sz);
  assertEqual(strcmp(result, expect), 0);

  call(out, close);
  return 0;
}
static int memory_read() {
  char src[] = "hello\0WORLD";
  Input* in = memory_input_new(src, sizeof(src));

  char cbuf[100];

  assertEqual(io_read(in, cbuf, 6), 6);
  assertEqual(strcmp(cbuf, "hello"), 0);
  assertEqual(io_read(in, cbuf, 6), 6);
  assertEqual(strcmp(cbuf, "WORLD"), 0);

  assertEqual(io_get(in), -1);

  call(in, close);
  return 0;
}
static int memory_output() {
  const char expect[] = "Hello World!";
  char buf[20];
  Output* out = memory_output_new(buf, sizeof(buf));
  io_write(out, expect, sizeof(expect));
  size_t sz = memory_output_size(out);
  assertEqual(sz, sizeof(expect));
  assertTrue(memcmp(buf, expect, sz) == 0);
  call(out, close);
  return 0;
}
static int memory_rewind() {
  char buf[20];
  Output* out = memory_output_new(buf, sizeof(buf));
  io_writelit(out, "Hello 0xF345A00D");
  memory_output_rewind(out, 6);
  io_writelit(out, "World!");
  io_put(out, 0);

  size_t sz = memory_output_size(out);
  const char expect[] = "Hello World!";
  assertEqual(sz, sizeof(expect));
  assertTrue(memcmp(buf, expect, sz) == 0);

  call(out, close);
  return 0;
}

static int limited_input() {
  const char* src = "Bob is cool NOT";
  Input* base = memory_input_new(src, strlen(src));
  Input* in = limited_input_new(base, 11);

  Output* strout = string_output_new(100);
  size_t copied = io_copy(in, strout);
  assertEqual(copied, 11);

  const char* expect = "Bob is cool";
  size_t sz;
  const char* result = string_output_data(strout, &sz);
  assertEqual(strlen(expect), sz);
  assertEqual(strcmp(expect, result), 0);

  call(strout, close);
  call(in, close);
  return 0;
}
static int limited_input_unget() {
  const char* src = "Bob is cool";
  Input* in = limited_input_new(memory_input_new(src, strlen(src)), 3);

  int c;
  c = io_get(in); assertEqual(c, 'B');
  c = io_get(in); assertEqual(c, 'o');
  c = io_get(in); assertEqual(c, 'b');
  io_unget(in);
  c = io_get(in); assertEqual(c, 'b');
  c = io_get(in); assertEqual(c, -1);

  call(in, close);
  return 0;
}

static int binary_io_utils() {
  Output* out = string_output_new(100);

  io_put_int8(out, 0x12);
  io_put_int16(out, 0x3456);
  io_put_int32(out, 0x7890ABCDL);
  io_put_int64(out, 0xDEADBEEF88997755L);

  size_t sz;
  const char* data = string_output_data(out, &sz);
  Input* in = memory_input_new(data, sz);

  assertEqual(0x12, io_get_int8(in));
  assertEqual(0x3456, io_get_int16(in));
  assertEqual(0x7890ABCDL, io_get_int32(in));
  assertEqual(0xDEADBEEF88997755L, io_get_int64(in));

  call(out, close);
  call(in, close);
  return 0;
}
static int formatting() {
  Output* out = string_output_new(100);

  IOFormatter fmt[1];
  io_format_init(fmt, out, 1);
  io_format(fmt, "Hello, %s! 12 * 6 = %d", "World", 72);
  io_writelit(out, " :D");
  io_format_close(fmt);

  const char* expect = "Hello, World! 12 * 6 = 72 :D";
  size_t sz;
  const char* result = string_output_data(out, &sz);
  assertEqual(sz, strlen(expect));
  assertTrue(memcmp(expect, result, strlen(expect)) == 0);

  call(out, close);
  return 0;
}

VLIB_SUITE(io) = {
  VLIB_TEST(string_io_write),
  VLIB_TEST(string_io_put),
  VLIB_TEST(string_io_reset),
  VLIB_TEST(memory_read),
  VLIB_TEST(memory_output),
  VLIB_TEST(memory_rewind),
  VLIB_TEST(limited_input),
  VLIB_TEST(limited_input_unget),
  VLIB_TEST(binary_io_utils),
  VLIB_TEST(formatting),
  VLIB_END,
};

