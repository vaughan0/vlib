#ifndef VARINT_H_EFE975ADE6C81F
#define VARINT_H_EFE975ADE6C81F

#include <stdlib.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/util.h>

data(Varint) {
  size_t  len;
  size_t  max;
  union {
    uint8_t digits[0];
    char    data[0];
  };
};

Varint* varint_new(size_t max_len);
static inline void varint_free(Varint* v) {
  free(v);
}

#define VARINT_STACK(name, size) STACK_ALLOC(Varint, name, sizeof(uint8_t)*(size)); name->max = (size);

uint64_t  varint_to_uint(Varint* v);
int64_t   varint_to_int(Varint* v);

void      uint_to_varint(uint64_t i, Varint* v);
void      int_to_varint(int64_t i, Varint* v);

// Attempts to decode a Varint. Returns true if successful, false if the varint was not created
// with enough capacity to hold the data. In either case, to->len will indicate the number of bytes read from src.
bool    varint_decode(Varint* to, Input* src);
bool    varint_decodestr(Varint* to, const char* str, size_t sz);

// After reading bytes into v->data and setting v->len, varint_validate can be used to check if the data is well-formed.
bool    varint_validate(Varint* v);

// Encodes a varint to an output stream. Exactly v->len bytes will be written.
void    varint_encode(Varint* v, Output* dst);

#endif /* VARINT_H_EFE975ADE6C81F */

