
#include <string.h>
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/io.h>
#include <vlib/bufio.h>

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
static int string_io_read() {
  char src[] = "hello\0WORLD";
  Input* in = string_input_new(src, sizeof(src));

  char cbuf[100];

  assertEqual(io_read(in, cbuf, 6), 6);
  assertEqual(strcmp(cbuf, "hello"), 0);
  assertEqual(io_read(in, cbuf, 6), 6);
  assertEqual(strcmp(cbuf, "WORLD"), 0);

  assertEqual(io_get(in), -1);

  call(in, close);
  return 0;
}

static int limited_input() {
  const char* src = "Bob is cool NOT";
  Input* base = string_input_new(src, strlen(src));
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
  Input* in = limited_input_new(string_input_new(src, strlen(src)), 3);

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

VLIB_SUITE(io) = {
  VLIB_TEST(string_io_write),
  VLIB_TEST(string_io_put),
  VLIB_TEST(string_io_read),
  VLIB_TEST(string_io_reset),
  VLIB_TEST(limited_input),
  VLIB_TEST(limited_input_unget),
  VLIB_END,
};

