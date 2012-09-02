
#include <vlib/test.h>

#define SUITE(name) extern VLIB_SUITE(name)
#include "suites.h"
#undef SUITE

int main() {
#define SUITE(name) VLIB_RUN_TESTS(name)
#include "suites.h"
#undef SUITE
  return test_results();
}

