
#include <stdlib.h>
#include <string.h>

#include <vlib/buffer.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

Buffer* buffer_new(size_t cap) {
  Buffer* self = malloc(sizeof(Buffer) + cap);
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

void buffer_fill(Buffer* self, RawInput* in) {
  self->read = 0;
  self->write = call(in, read, self->data, self->size);
}

void buffer_flush(Buffer* self, RawOutput* out) {
  call(out, write, self->data + self->read, self->write - self->read);
  self->write = 0;
  self->read = 0;
}

void buffer_reset(Buffer* self) {
  self->read = 0;
  self->write = 0;
}
