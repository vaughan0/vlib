#ifndef HEAP_H_CEF19AB4CFA72A
#define HEAP_H_CEF19AB4CFA72A

#include <stddef.h>
#include <stdint.h>

#include <vlib/vector.h>

// Comparison function. Returns a non-zero integer if a has higher priority than b.
typedef int (*HeapCmp)(const void* a, const void* b);

typedef int HeapRef;

typedef struct Heap {
  HeapCmp     cmp;
  size_t      elemsz;
  size_t      size;
  Vector      v;
} Heap;

void      heap_init(Heap* h, HeapCmp cmp, size_t elemsz, size_t capacity);
void      heap_close(Heap* h);

void*     heap_peek(Heap* h, HeapRef* ref);
void      heap_pop(Heap* h);

/* To push a value to a heap:
 * First obtain a pointer to the new value with heap_alloc(),
 * then call heap_push() so that the heap can restructure itself.
 */
void*     heap_alloc(Heap* h);
HeapRef   heap_push(Heap* h);

void*     heap_get(Heap* h, HeapRef ref);
void      heap_remove(Heap* h, HeapRef ref);
void      heap_update(Heap* h, HeapRef ref);

// Iterates through the heap, calling the callback with a pointer to each element.
// If the callback returns false at any point, then the iteration will stop and heap_iter will return.
void      heap_iter(Heap* h, bool (*callback)(void* elem));

#endif /* HEAP_H_CEF19AB4CFA72A */

