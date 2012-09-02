
#include <vlib/test.h>

#include "vector.c"

int main() {
  VLIB_RUN_TESTS(vector);
  return test_results();
}

