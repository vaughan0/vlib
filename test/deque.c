
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlib/test.h>
#include <vlib/deque.h>

static int deque_basic() {
  Deque d[1];
  deque_init(d, sizeof(int), 10);

  *(int*)deque_pushback(d) = 2;
  *(int*)deque_pushback(d) = 3;
  *(int*)deque_pushfront(d) = 1;
  *(int*)deque_pushback(d) = 4;
  *(int*)deque_pushfront(d) = 0;

  for (int i = 0; i <= 4; i++) {
    int val = *(int*)deque_front(d);
    deque_popfront(d);
    assertEqual(i, val);
    assertEqual(4-i, deque_size(d));
  }

  deque_close(d);
  return 0;
}

VLIB_SUITE(deque) = {
  VLIB_TEST(deque_basic),
  VLIB_END,
};
