
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/bitset.h>

static int bitset_basic() {
  size_t n = 200;
  Bitset* bits = malloc(bitset_size(n));
  bitset_init(bits, n);

  for (unsigned i = 0; i < n; i++) {
    bitset_set(bits, i, (i % 3 == 0));
  }
  for (unsigned i = 0; i < n; i++) {
    assertEqual(bitset_get(bits, i), (i % 3 == 0));
  }

  free(bits);
  return 0;
}

VLIB_SUITE(bitset) = {
  VLIB_TEST(bitset_basic),
  VLIB_END
};
