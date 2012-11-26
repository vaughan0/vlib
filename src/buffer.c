
#include <stdlib.h>
#include <string.h>

#include <vlib/buffer.h>
#include <vlib/util.h>
#include <vlib/error.h>

Buffer* buffer_new(size_t cap) {
  Buffer* self = v_malloc(sizeof(Buffer) + cap);
  self->size = cap;
  self->read = 0;
  self->write = 0;
  return self;
}
void buffer_free(Buffer* self) {
  free(self);
}

size_t buffer_write(Buffer* self, const char* data, size_t n) {
  n = MIN(n, buffer_avail_write(self));
  memcpy(self->data + self->write, data, n);
  self->write += n;
  return n;
}

size_t buffer_read(Buffer* self, char* dst, size_t n) {
  n = MIN(n, buffer_avail_read(self));
  memcpy(dst, self->data + self->read, n);
  self->read += n;
  return n;
}

void buffer_fill(Buffer* self, Input* in) {
  self->read = 0;
  int64_t r = io_read(in, self->data, self->size);
  if (r < 0) verr_raise(VERR_EOF);
  self->write = r;
}

void buffer_flush(Buffer* self, Output* out) {
  io_write(out, self->data + self->read, self->write - self->read);
  self->write = 0;
  self->read = 0;
}

void buffer_reset(Buffer* self) {
  self->read = 0;
  self->write = 0;
}
