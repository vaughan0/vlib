
#include <stdlib.h>
#include <assert.h>

#include <vlib/llist.h>
#include <vlib/error.h>

data(Node) {
  Node  *prev, *next;
  char  data[0];
};

void  llist_init(LList* l, size_t elemsz) {
  l->size = 0;
  l->elemsz = elemsz;
  l->_first = l->_last = NULL;
}
void  llist_close(LList* l) {
  Node *node, *tmp;
  for (node = l->_first; node; node = tmp) {
    tmp = node->next;
    free(node);
  }
}

void* llist_front(LList* l) {
  assert(l->_first);
  return ((Node*)l->_first)->data;
}

void* llist_push_front(LList* l) {
  Node* n = malloc(sizeof(Node) + l->elemsz);
  l->size++;
  n->prev = NULL;
  n->next = l->_first;
  if (n->next) n->next->prev = n;
  l->_first = n;
  if (!l->_last) l->_last = n;
  return n->data;
}

void  llist_pop_front(LList* l) {
  assert(l->_first);
  l->size--;
  Node* n = l->_first;
  l->_first = n->next;
  *(n->next ? &n->next->prev : (Node**)&l->_last) = NULL;
  free(n);
}

void* llist_back(LList* l) {
  assert(l->_last);
  return ((Node*)l->_last)->data;
}

void* llist_push_back(LList* l) {
  l->size++;
  Node* n = malloc(sizeof(Node) + l->elemsz);
  n->next = NULL;
  n->prev = l->_last;
  if (n->prev) n->prev->next = n;
  l->_last = n;
  if (!l->_first) l->_first = n;
  return n->data;
}

void  llist_pop_back(LList* l) {
  assert(l->_last);
  l->size--;
  Node* n = l->_last;
  l->_last = n->prev;
  *(n->prev ? &n->prev->next : (Node**)&l->_first) = NULL;
  free(n);
}

void  llist_iter(LList* l, bool forward, int (*callback)(void* elem)) {
  Node* node = forward ? l->_first : l->_last;
  Node* next;
  for (; node; node = next) {
    next = forward ? node->next : node->prev;
    int r = callback(node->data);

    if (r & LLIST_REMOVE) {
      l->size--;
      *(node->prev ? &node->prev->next : (Node**)&l->_first) = node->next;
      *(node->next ? &node->next->prev : (Node**)&l->_last) = node->prev;
      free(node);
    }

    if ((r & 1) == LLIST_BREAK) break;
  }
}
