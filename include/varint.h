#ifndef VARINT_H_EFE975ADE6C81F
#define VARINT_H_EFE975ADE6C81F

#include <stdlib.h>

#include <vlib/std.h>
#include <vlib/io.h>

data(Varint) {
  size_t  len;
  size_t  max;
  uint8_t digits[];
};

Varint* varint_new(size_t max_len);
static inline void varint_free(Varint* v) {
  free(v);
}

uint64_t  varint_to_uint(Varint* v);
void      uint_to_varint(uint64_t i, Varint* v);

int64_t   varint_to_int(Varint* v);
void      int_to_varint(int64_t i, Varint* v);

// Attempts to decode a Varint. Returns true if successful, false if the varint was not created
// with enough capacity to hold the data. In either case, to->len will indicate the number of bytes read from src.
bool    varint_decode(Varint* to, Input* src);

// Encodes 
void    varint_encode(Varint* v, Output* dst);

#endif /* VARINT_H_EFE975ADE6C81F */

