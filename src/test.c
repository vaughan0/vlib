
#include <stdio.h>
#include <stdarg.h>

#include <vlib/test.h>

static unsigned stat_total;
static unsigned stat_passed;

static char errbuf[100];
void test_error(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vsnprintf(errbuf, sizeof(errbuf), fmt, va);
  va_end(va);
}

void test_suite(const char* name, Test tests[], unsigned ntests) {
  printf("%s\n", name);
  for (unsigned i = 0; i < ntests; i++) {
    stat_total++;
    printf("  %s... ", tests[i].name);
    int r = tests[i].code();
    if (r) {
      printf("FAIL: %s\n", errbuf);
    } else {
      printf("PASS\n");
      stat_passed++;
    }
  }
}

int test_results() {
  printf("\n");
  if (stat_passed == stat_total) {
    printf("All tests PASSED\n");
    return 0;
  } else {
    printf("%d/%d tests FAILED\n", (stat_total-stat_passed), stat_total);
    return 1;
  }
}
