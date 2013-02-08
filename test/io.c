
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

VLIB_SUITE(io) = {
  VLIB_TEST(string_io_write),
  VLIB_TEST(string_io_read),
  VLIB_TEST(string_io_reset),
  VLIB_END,
};

