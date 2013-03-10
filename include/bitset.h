#ifndef VLIB_BITSET_H
#define VLIB_BITSET_H

#include <vlib/std.h>

data(Bitset) {
  size_t    size;
  size_t    _n;
  uint64_t  words[];
};

static inline size_t bitset_size(size_t num_bits) {
  return sizeof(Bitset) + ((num_bits+63)/64) * sizeof(uint64_t);
}

void    bitset_init(Bitset* self, size_t num_bits);
void    bitset_reset(Bitset* self);

void    bitset_set(Bitset* self, unsigned index, bool value);
bool    bitset_get(Bitset* self, unsigned index);

#endif
