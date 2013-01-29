
#include <assert.h>
#include <string.h>

#include <vlib/error.h>
#include <vlib/deque.h>

void deque_init(Deque* d, size_t elemsz, size_t capacity) {
  d->elemsz = elemsz;
  d->_cap = capacity;
  d->_frontr = 0;
  d->_backw = 0;
  d->_data = NULL;
  d->_data = v_malloc(elemsz * capacity);
}
void deque_close(Deque* d) {
  if (d->_data) free(d->_data);
}

size_t deque_size(Deque* d) {
  if (d->_frontr <= d->_backw) {
    return d->_backw - d->_frontr;
  } else {
    return d->_cap - (d->_frontr - d->_backw);
  }
}

static inline int fix(Deque* d, int index) {
  if (index == d->_cap) return 0;
  if (index == -1) return d->_cap - 1;
  return index;
}

static inline void check_cap(Deque* d) {
  if (fix(d, d->_frontr-1) == d->_backw) {
    size_t newcap = d->_cap * 2;
    d->_data = v_realloc(d->_data, newcap * d->elemsz);
    if (d->_frontr > d->_backw) {
      // Shift elements to the end of the new memory
      int newfront = d->_frontr + d->_cap;
      memcpy(d->_data + newfront * d->elemsz, d->_data + d->_frontr * d->elemsz, (d->_cap - d->_frontr) * d->elemsz);
      d->_frontr = newfront;
    }
    d->_cap = newcap;
  }
}

void* deque_pushback(Deque* d) {
  check_cap(d);
  void* ptr = d->_data + (d->_backw * d->elemsz);
  d->_backw = fix(d, d->_backw+1);
  return ptr;
}
void* deque_back(Deque* d) {
  return d->_data + (fix(d, d->_backw-1) * d->elemsz);
}
void deque_popback(Deque* d) {
  assert(d->_backw != d->_frontr);
  d->_backw = fix(d, d->_backw-1);
}

void* deque_pushfront(Deque* d) {
  check_cap(d);
  d->_frontr = fix(d, d->_frontr-1);
  return d->_data + (d->_frontr * d->elemsz);
}
void* deque_front(Deque* d) {
  return d->_data + (d->_frontr * d->elemsz);
}
void deque_popfront(Deque* d) {
  assert(d->_backw != d->_frontr);
  d->_frontr = fix(d, d->_frontr+1);
}
