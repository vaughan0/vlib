#ifndef TEST_H_050A87E8F0E798
#define TEST_H_050A87E8F0E798

#include <stdio.h>

typedef struct Test {
  const char* name;
  int (*code)();
} Test;

void test_error(const char* file, int line, const char* fmt, ...);

#define failMsg(msg, ...) {test_error(__FILE__, __LINE__, msg, ##__VA_ARGS__); return 1;}

#define assertTrue(expr) {if (!(expr)) failMsg("%s was not true", #expr);}
#define assertFalse(expr) {if (expr) failMsg("%s was not false", #expr);}
#define assertEqual(a, b) {if ((a) != (b)) failMsg("%s != %s", #a, #b);}
#define assertNotEqual(a, b) {if ((a) == (b)) failMsg("%s == %s", #a, #b);}

#define VLIB_RUN_TESTS(suite) test_suite(#suite, _vlib_suite_##suite)

#define VLIB_SUITE(name) Test _vlib_suite_##name[]
#define VLIB_TEST(name) {#name, name}
#define VLIB_END {NULL, NULL}

int test_results();
void test_suite(const char* name, Test tests[]);

#endif /* TEST_H_050A87E8F0E798 */

