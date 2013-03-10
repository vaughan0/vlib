
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <vlib/bitset.h>

void bitset_init(Bitset* self, size_t num_bits) {
  self->size = num_bits;
  self->_n = (num_bits+63)/64;
  bitset_reset(self);
}
void bitset_reset(Bitset* self) {
  memset(self->words, 0, self->_n * sizeof(uint64_t));
}

void bitset_set(Bitset* self, unsigned index, bool value) {
  unsigned word = (index & (~0x3F)) >> 6;
  unsigned bit = index & 0x3F;
  assert(word < self->_n);
  uint64_t* ptr = &self->words[word];
  if (value)
    *ptr |= (1UL << bit);
  else
    *ptr &= ~(1UL << bit);
}
bool bitset_get(Bitset* self, unsigned index) {
  unsigned word = (index & (~0x3F)) >> 6;
  unsigned bit = index & 0x3F;
  assert(word < self->_n);
  uint64_t bits = self->words[word];
  return (bits & (1UL << bit)) ? true : false;
}
