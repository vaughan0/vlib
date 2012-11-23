
#include <vlib/test.h>
#include <vlib/llist.h>

static inline void pushfront(LList* l, int val) {
  *(int*)llist_push_front(l) = val;
}
static inline void pushback(LList* l, int val) {
  *(int*)llist_push_back(l) = val;
}

static inline int popfront(LList* l) {
  int val = *(int*)llist_front(l);
  llist_pop_front(l);
  return val;
}
static inline int popback(LList* l) {
  int val = *(int*)llist_back(l);
  llist_pop_back(l);
  return val;
}

static int llist_basic() {
  LList l[1];
  llist_init(l, sizeof(int));

  pushfront(l, 3);
  pushback(l, 4);
  pushfront(l, 2);
  pushfront(l, 1);
  pushback(l, 5);

  assertEqual(l->size, 5);
  assertEqual(popfront(l), 1);
  assertEqual(popfront(l), 2);
  assertEqual(popfront(l), 3);
  assertEqual(popfront(l), 4);
  assertEqual(popfront(l), 5);
  assertEqual(l->size, 0);

  llist_close(l);
  return 0;
}

static int llist_iteration() {
  LList l[1];
  llist_init(l, sizeof(int));

  for (int i = 1; i <= 10; i++) {
    pushback(l, i);
  }

  int remove_even(void* data) {
    int val = *(int*)data;
    return (val % 2 == 0) ? LLIST_REMOVE : 0;
  }
  llist_iter(l, true, remove_even);

  int double_vals(void* data) {
    int* ival = data;
    *ival *= 2;
    return 0;
  }
  llist_iter(l, false, double_vals);

  assertEqual(l->size, 5);
  assertEqual(popback(l), 18);
  assertEqual(popback(l), 14);
  assertEqual(popback(l), 10);
  assertEqual(popback(l), 6);
  assertEqual(popback(l), 2);
  assertEqual(l->size, 0);

  llist_close(l);
  return 0;
};

VLIB_SUITE(llist) = {
  VLIB_TEST(llist_basic),
  VLIB_TEST(llist_iteration),
  VLIB_END,
};
