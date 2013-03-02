
#include <stdio.h>
#include <string.h>

#include <vlib/test.h>
#include <vlib/varint.h>

static int varint_basic() {
  Varint* v = varint_new(8);
  uint64_t tests[] = {0, 1, 2, 3, 126, 127, 128, 129, 1234567, 2097151, 2097152, 2097153};
  for (unsigned i = 0; i < sizeof(tests)/sizeof(uint64_t); i++) {
    uint_to_varint(tests[i], v);
    uint64_t result = varint_to_uint(v);
    assertEqual(result, tests[i]);
  }
  varint_free(v);
  return 0;
}

static int varint_signed() {
  Varint* v = varint_new(8);
  int64_t tests[] = {0, 1, 2, 3, -1, -2, -3, 2097152, -2097152};
  for (unsigned i = 0; i < sizeof(tests)/sizeof(int64_t); i++) {
    int_to_varint(tests[i], v);
    int64_t result = varint_to_int(v);
    assertEqual(result, tests[i]);
  }
  varint_free(v);
  return 0;
}

data(VarintTest) {
  uint64_t    integral;
  const char* binary;
};

static VarintTest encoding_tests[] = {
  {1,             "\x01"},
  {2,             "\x02"},
  {1 + 1*128,     "\x81\x01"},
  {2 + 1*128,     "\x82\x01"},
  {1 + 2*128,     "\x81\x02"},
  {4 + 3*128*128, "\x84\x80\x03"},
};

static int varint_encoding() {
  Varint* v = varint_new(8);
  Output* out = string_output_new(64);
  for (unsigned i = 0; i < sizeof(encoding_tests)/sizeof(Test); i++) {
    VarintTest test = encoding_tests[i];
    uint_to_varint(test.integral, v);
    string_output_reset(out);
    varint_encode(v, out);
    
    size_t sz;
    const char* result = string_output_data(out, &sz);
    unsigned j;
    for (j = 0; test.binary[j]; j++) {
      uint8_t got = result[j], expect = test.binary[j];
      assertTrue(j < sz);
      assertEqual(expect, got);
    }
    assertEqual(j, sz);
  }
  call(out, close);
  varint_free(v);
  return 0;
}

static int varint_decoding() {
  Varint* v = varint_new(8);
  Input* in = memory_input_new(NULL, 0);
  for (unsigned i = 0; i < sizeof(encoding_tests)/sizeof(Test); i++) {
    VarintTest test = encoding_tests[i];
    memory_input_reset(in, test.binary, strlen(test.binary));
    assertEqual(true, varint_decode(v, in));
    uint64_t result = varint_to_uint(v);
    assertEqual(result, test.integral);
  }
  call(in, close);
  varint_free(v);
  return 0;
}

VLIB_SUITE(varint) = {
  VLIB_TEST(varint_basic),
  VLIB_TEST(varint_signed),
  VLIB_TEST(varint_encoding),
  VLIB_TEST(varint_decoding),
  VLIB_END,
};

