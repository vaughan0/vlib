#ifndef DEQUE_H_95CF9713056D46
#define DEQUE_H_95CF9713056D46

#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>

data(Deque) {
  size_t    elemsz;
  size_t    _cap;
  int       _frontr;
  int       _backw;
  char*     _data;
};

void deque_init(Deque* d, size_t elemsz, size_t capacity);
void deque_close(Deque* d);

size_t deque_size(Deque* d);

static inline bool deque_empty(Deque* d) {
  return d->_frontr == d->_backw;
}

void* deque_pushfront(Deque* d);
void* deque_front(Deque* d);
void  deque_popfront(Deque* d);

void* deque_pushback(Deque* d);
void* deque_back(Deque* d);
void  deque_popback(Deque* d);

#endif /* DEQUE_H_95CF9713056D46 */

