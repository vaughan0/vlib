#ifndef BUFFER_H_692089ACEF85E3
#define BUFFER_H_692089ACEF85E3

#include <stddef.h>

#include <vlib/std.h>
#include <vlib/io.h>

// A simple, non-circular buffer. Primary used for buffered IO.
data(Buffer) {
  size_t  size;
  size_t  read;
  size_t  write;
  char    data[0];
};

Buffer*   buffer_new(size_t cap);
void      buffer_free(Buffer* buf);

static inline size_t buffer_avail_read(Buffer* self) {
  return self->write - self->read;
}
static inline size_t buffer_avail_write(Buffer* self) {
  return self->size - self->write;
}

size_t    buffer_write(Buffer* self, const char* data, size_t n);
size_t    buffer_read(Buffer* self, char* dst, size_t n);

void      buffer_fill(Buffer* self, RawInput* in);
void      buffer_flush(Buffer* self, RawOutput* out);

void      buffer_reset(Buffer* self);

#endif /* BUFFER_H_692089ACEF85E3 */

