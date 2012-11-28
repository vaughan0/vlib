
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/bufio.h>

/* Input */

static Input_Impl buf_input_impl;

void* buf_input_new(Input* wrap, size_t buffer) {
  BufInput* self = v_malloc(sizeof(BufInput));
  self->base._impl = &buf_input_impl;
  self->in = wrap;
  self->buf = buffer_new(buffer);
  self->close_backend = true;
  return self;
}

static void buf_input_close(void* _self) {
  BufInput* self = _self;
  buffer_free(self->buf);
  if (self->close_backend)
    call(self->in, close);
  free(self);
}

static size_t buf_input_read(void* _self, char* dst, size_t n) {
  BufInput* self = _self;
  if (buffer_avail_read(self->buf) == 0) {
    buffer_fill(self->buf, self->in);
  }
  return buffer_read(self->buf, dst, n);
}

static int buf_input_get(void* _self) {
  BufInput* self = _self;
  if (!buffer_avail_read(self->buf)) {
    buffer_fill(self->buf, self->in);
    if (!buffer_avail_read(self->buf)) {
      return -1;
    }
  }
  return self->buf->data[self->buf->read++];
}

static void buf_input_unget(void* _self) {
  BufInput* self = _self;
  self->buf->read--;
}

static bool buf_input_eof(void* _self) {
  BufInput* self = _self;
  if (buffer_avail_read(self->buf)) {
    return false;
  }
  return io_eof(self->in);
}

static Input_Impl buf_input_impl = {
  .read = buf_input_read,
  .get = buf_input_get,
  .unget = buf_input_unget,
  .eof = buf_input_eof,
  .close = buf_input_close,
};

/* Output */

static Output_Impl buf_output_impl;

void* buf_output_new(Output* wrap, size_t buffer) {
  BufOutput* self = v_malloc(sizeof(BufOutput));
  self->base._impl = &buf_output_impl;
  self->out = wrap;
  self->buf = buffer_new(buffer);
  self->close_backend = true;
  return self;
}

static void buf_output_write(void* _self, const char* src, size_t n) {
  BufOutput* self = _self;
  if (buffer_avail_write(self->buf) >= n) {
    buffer_write(self->buf, src, n);
  } else {
    buffer_flush(self->buf, self->out);
    call(self->out, write, src, n);
  }
}

static void buf_output_put(void* _self, char ch) {
  BufOutput* self = _self;
  if (buffer_avail_write(self->buf) == 0) {
    buffer_flush(self->buf, self->out);
  }
  self->buf->data[self->buf->write++] = ch;
}

static void buf_output_flush(void* _self) {
  BufOutput* self = _self;
  buffer_flush(self->buf, self->out);
  call(self->out, flush);
}

static void buf_output_close(void* _self) {
  BufOutput* self = _self;
  buf_output_flush(self);
  buffer_free(self->buf);
  if (self->close_backend)
    call(self->out, close);
  free(self);
}

static Output_Impl buf_output_impl = {
  .write = buf_output_write,
  .put = buf_output_put,
  .flush = buf_output_flush,
  .close = buf_output_close,
};
