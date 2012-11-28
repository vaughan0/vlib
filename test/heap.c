
#include <vlib/test.h>
#include <vlib/heap.h>

static int int_cmp(const void* a, const void* b) {
  return *(int*)a > *(int*)b;
}
static void iheap_init(Heap* h) {
  heap_init(h, int_cmp, sizeof(int), 1);
}
static HeapRef ipush(Heap* h, int i) {
  int* ptr = heap_alloc(h);
  *ptr = i;
  return heap_push(h);
}
static int ipop(Heap* h) {
  int val = *(int*)heap_peek(h, NULL);
  heap_pop(h);
  return val;
}
static void iupdate(Heap* h, HeapRef ref, int val) {
  *(int*)heap_get(h, ref) = val;
  heap_update(h, ref);
}
static int iget(Heap* h, HeapRef ref) {
  return *(int*)heap_get(h, ref);
}

static int heap_basic() {
  Heap h;
  iheap_init(&h);

  ipush(&h, 3);
  ipush(&h, 5);
  ipush(&h, 2);
  ipush(&h, 0);
  ipush(&h, 1);
  ipush(&h, 6);
  ipush(&h, 4);

  assertEqual(ipop(&h), 6);
  assertEqual(ipop(&h), 5);
  assertEqual(ipop(&h), 4);
  assertEqual(ipop(&h), 3);
  assertEqual(ipop(&h), 2);
  assertEqual(ipop(&h), 1);
  assertEqual(ipop(&h), 0);

  heap_close(&h);
  return 0;
}

static int heap_refs() {
  Heap h;
  iheap_init(&h);

  HeapRef refs[10];
  for (int i = 0; i < 10; i++) {
    refs[i] = ipush(&h, 9-i);
  }
  // refs: 0 1 2 3 4 5 6 7 8 9
  // vals: 9 8 7 6 5 4 3 2 1 0

  for (int i = 5; i <= 8; i++) {
    int val = iget(&h, refs[i]);
    iupdate(&h, refs[i], val+2);
  }
  // refs: 0 1 2 3 4 5 6 7 8 9
  // vals: 9 8 7 6 5 6 5 4 3 0

  for (int i = 1; i <= 2; i++) {
    heap_remove(&h, refs[i]);
  }
  // refs: 0 3 4 5 6 7 8 9
  // vals: 9 6 5 6 5 4 3 0

  for (int i = 0; i < 2; i++) {
    HeapRef ref = ipush(&h, i);
    iupdate(&h, ref, i+1);
  }
  // refs: 0 3 4 5 6 7 8 9
  // vals: 9 6 5 6 5 4 3 0 1 2

  assertEqual(iget(&h, refs[0]), 9);
  /* removed refs 1 and 2 */
  assertEqual(iget(&h, refs[3]), 6);
  assertEqual(iget(&h, refs[4]), 5);
  assertEqual(iget(&h, refs[5]), 6);
  assertEqual(iget(&h, refs[6]), 5);
  assertEqual(iget(&h, refs[7]), 4);
  assertEqual(iget(&h, refs[8]), 3);
  assertEqual(iget(&h, refs[9]), 0);

  assertEqual(ipop(&h), 9);
  assertEqual(ipop(&h), 6);
  assertEqual(ipop(&h), 6);
  assertEqual(ipop(&h), 5);
  assertEqual(ipop(&h), 5);
  assertEqual(ipop(&h), 4);
  assertEqual(ipop(&h), 3);
  assertEqual(ipop(&h), 2);
  assertEqual(ipop(&h), 1);
  assertEqual(ipop(&h), 0);

  heap_close(&h);
  return 0;
}

VLIB_SUITE(heap) = {
  VLIB_TEST(heap_basic),
  VLIB_TEST(heap_refs),
  VLIB_END,
};
