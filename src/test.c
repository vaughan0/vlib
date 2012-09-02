
#include <stdio.h>
#include <stdarg.h>

#include <vlib/test.h>

static unsigned stat_total;
static unsigned stat_passed;

static char errbuf[200];
void test_error(const char* file, int line, const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int n = snprintf(errbuf, sizeof(errbuf), "%s:%d: ", file, line);
  vsnprintf(errbuf+n, sizeof(errbuf)-n, fmt, va);
  va_end(va);
}

void test_suite(const char* name, Test tests[]) {
  printf("%s\n", name);
  for (unsigned i = 0; tests[i].name; i++) {
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
