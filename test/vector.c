
#include <vlib/test.h>
#include <vlib/vector.h>

static int vector_basic() {
  Vector v;
  vector_init(&v, sizeof(int), 1);
  for (int i = 0; i < 10; i++) {
    int* ptr = vector_push(&v);
    *ptr = i+1;
  }
  for (int i = 0; i < 10; i++) {
    int* ptr = vector_get(&v, i);
    assertEqual(i+1, *ptr);
  }
  for (int i = 0; i < 10; i++) {
    vector_pop(&v);
  }
  assertEqual(v.size, 0);
  vector_close(&v);
  return 0;
}

VLIB_SUITE(vector) = {
  VLIB_TEST(vector_basic),
  VLIB_END,
};
