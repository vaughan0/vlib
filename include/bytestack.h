#ifndef VLIB_BYTESTACK_H
#define VLIB_BYTESTACK_H

#include <vlib/std.h>
#include <vlib/vector.h>

data(ByteStack) {
  size_t    cap;
  size_t    size;
  char*     data;
  Vector    stack[1];
};

void  bytestack_init(ByteStack* self, size_t init_cap);
void  bytestack_close(ByteStack* self);

// Allocates a new block of `size` bytes and returns a pointer to it.
void* bytestack_push(ByteStack* self, size_t size);

// Returns a pointer to the most recently allocated block of data.
void* bytestack_top(ByteStack* self);

// Pops the most recently allocated block of data.
void  bytestack_pop(ByteStack* self);

#endif
