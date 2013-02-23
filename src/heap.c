
#include <assert.h>
#include <stdio.h>

#include <vlib/heap.h>

typedef struct Data {
  // vector[index] fields:
  HeapRef   ref;
  // vector[ref] fields:
  unsigned  index;
  char      data[0];
} Data;

void heap_init(Heap* h, HeapCmp cmp, size_t elemsz, size_t cap) {
  h->cmp = cmp;
  h->elemsz = elemsz;
  h->size = 0;
  vector_init(&h->v, sizeof(Data) + elemsz, cap);
}
void heap_close(Heap* h) {
  vector_close(&h->v);
}

static inline void* get_data(Heap* h, unsigned index) {
  HeapRef ref = ((Data*)vector_get(&h->v, index))->ref;
  return ((Data*)vector_get(&h->v, ref))->data;
}
static inline void swap(Heap* h, unsigned ai, unsigned bi) {
  Data* a = vector_get(&h->v, ai);
  Data* b = vector_get(&h->v, bi);
  // Swap references
  HeapRef aref = a->ref;
  HeapRef bref = b->ref;
  a->ref = bref;
  b->ref = aref;
  // Swap indices
  ((Data*)vector_get(&h->v, aref))->index = bi;
  ((Data*)vector_get(&h->v, bref))->index = ai;
}

static void siftup(Heap* h, unsigned child) {
  while (child > 0) {
    unsigned parent = child / 2;
    if (!h->cmp(get_data(h, child), get_data(h, parent))) {
      break;
    }
    swap(h, child, parent);
    child = parent;
  }
}
static void siftdown(Heap* h, unsigned parent) {
  while (parent*2 + 1 < h->size) {
    unsigned child = parent*2 + 1;
    if (child+1 < h->size && h->cmp(get_data(h, child+1), get_data(h, child))) {
      child++;
    }
    if (!h->cmp(get_data(h, child), get_data(h, parent))) {
      break;
    }
    swap(h, child, parent);
    parent = child;
  }
}

static void hremove(Heap* h, unsigned index) {
  swap(h, index, h->size-1);
  h->size--;
  siftdown(h, index);
}

void* heap_alloc(Heap* h) {
  Data* d;
  if (h->size == h->v.size) {
    // no free Data objects
    d = vector_push(&h->v);
    d->ref = h->size;
  } else {
    // re-use a Data object
    d = vector_get(&h->v, h->size);
  }
  Data* refd = vector_get(&h->v, d->ref);
  refd->index = h->size;
  h->size++;
  return refd->data;
}
HeapRef heap_push(Heap* h) {
  assert(h->size > 0);
  Data* d = vector_get(&h->v, h->size-1);
  HeapRef ref = d->ref;
  siftup(h, h->size-1);
  return ref;
}

void* heap_peek(Heap* h, HeapRef* ref) {
  assert(h->size > 0);
  if (ref) *ref = ((Data*)vector_get(&h->v, 0))->ref;
  return get_data(h, 0);
}
void heap_pop(Heap* h) {
  assert(h->size > 0);
  hremove(h, 0);
}

void* heap_get(Heap* h, HeapRef ref) {
  assert(ref >= 0);
  return ((Data*)vector_get(&h->v, ref))->data;
}
void heap_remove(Heap* h, HeapRef ref) {
  assert(ref >= 0);
  unsigned index = ((Data*)vector_get(&h->v, ref))->index;
  hremove(h, index);
}
void heap_update(Heap* h, HeapRef ref) {
  unsigned index = ((Data*)vector_get(&h->v, ref))->index;
  siftup(h, index);
  siftdown(h, index);
}

void heap_iter(Heap* h, bool (*callback)(void* elem)) {
  for (unsigned i = 0; i < h->size; i++) {
    void* data = get_data(h, i);
    if (!callback(data)) break;
  }
}
