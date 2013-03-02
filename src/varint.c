
#include <assert.h>

#include <vlib/varint.h>
#include <vlib/error.h>

Varint* varint_new(size_t max_len) {
  assert(max_len > 0);
  Varint* v = malloc(sizeof(Varint) + max_len);
  v->len = 0;
  v->max = max_len;
  return v;
}

uint64_t varint_to_uint(Varint* v) {
  assert(v->len > 0);
  uint64_t n = 0;
  for (int i = v->len-1; i >= 0; i--) {
    n = (n << 7) | (v->digits[i] & 0x7F);
  }
  return n;
}
void uint_to_varint(uint64_t n, Varint* v) {
  if (n == 0) {
    v->len = 1;
    v->digits[0] = 0;
    return;
  }
  v->len = 0;
  for (;;) {
    uint8_t digit = n & 0x7F;
    n = n >> 7;
    if (n) {
      v->digits[v->len++] = 0x80 | digit;
    } else {
      v->digits[v->len++] = digit;
      break;
    }
    if (v->len == v->max) {
      v->len = 0;
      RAISE(NOMEM);
    }
  }
}

int64_t varint_to_int(Varint* v) {
  uint64_t n = varint_to_uint(v);
  if (n & 1) {
    return -(int64_t)((n+1) / 2);
  } else {
    return (int64_t)(n / 2);
  }
}
void int_to_varint(int64_t n, Varint* v) {
  if (n < 0) {
    uint_to_varint((uint64_t)(-n) * 2 - 1, v);
  } else {
    uint_to_varint((uint64_t)n * 2, v);
  }
}

static inline bool decode_with(Varint* v, uint8_t (*get)()) {
  v->len = 0;
  for (;;) {
    uint8_t digit = get();
    v->digits[v->len++] = digit;
    if ((digit & 0x80) == 0) return true;
    if (v->len == v->max) return false;
  }
}
bool varint_decode(Varint* to, Input* src) {
  uint8_t get() {
    int c = io_get(src);
    if (c == -1) RAISE(EOF);
    assert(c >= 0 && c < 256);
    return c;
  }
  return decode_with(to, get);
}
bool varint_decodestr(Varint* to, const char* str, size_t sz) {
  size_t offset = 0;
  uint8_t get() {
    if (offset == sz) RAISE(EOF);
    return (uint8_t)str[offset++];
  }
  return decode_with(to, get);
}

bool varint_validate(Varint* v) {
  if (v->len == 0) return false;
  for (unsigned i = 0; i < v->len-1; i++)
    if (!(v->digits[i] & 0x80)) return false;
  if (v->digits[v->len-1] & 0x80) return false;
  return true;
}

void varint_encode(Varint* v, Output* out) {
  assert(v->len > 0);
  io_write(out, v->data, v->len);
}
