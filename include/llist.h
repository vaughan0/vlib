#ifndef LIST_H_FF9E9C9DC110E0
#define LIST_H_FF9E9C9DC110E0

#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>

data(LList) {
  size_t  size;   // number of elements
  size_t  elemsz; // size of each element
  void*   _first;
  void*   _last;
};

void llist_init(LList* l, size_t elemsz);
void llist_close(LList* l);

void* llist_front(LList* l);
void* llist_push_front(LList* l);
void  llist_pop_front(LList* l);

void* llist_back(LList* l);
void* llist_push_back(LList* l);
void  llist_pop_back(LList* l);

/* Iteration */

/**
 * Iterates through the linked list. For each element, the callback will be called
 * with a pointer to the element data. The return value of the callback must be one
 * of LLIST_CONTINUE or LLIST_BREAK, with LLIST_REMOVE optionally bitwise-or'd in.
 */
void  llist_iter(LList* l, bool forward, int (*callback)(void* elem));

// llist_iter callback return value
enum {
  LLIST_CONTINUE  = 0,  // continue iterating
  LLIST_BREAK     = 1,  // stop iterating
  LLIST_REMOVE    = 2,  // used in combination with above two: remove element, then continue/break
};

#endif /* LIST_H_FF9E9C9DC110E0 */

