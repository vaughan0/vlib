
#include <stdlib.h>
#include <string.h>

#include <vlib/bufio.h>

/* Input */

static Input_Class buf_input_class;

void* buf_input_new(Input* wrap, size_t buffer) {
  BufInput* self = malloc(sizeof(BufInput));
  self->base._class = &buf_input_class;
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

static int64_t buf_input_read(void* _self, char* dst, size_t n) {
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

static Input_Class buf_input_class = {
  .read = buf_input_read,
  .get = buf_input_get,
  .unget = buf_input_unget,
  .close = buf_input_close,
};

/* Output */

static Output_Class buf_output_class;

void* buf_output_new(Output* wrap, size_t buffer) {
  BufOutput* self = malloc(sizeof(BufOutput));
  self->base._class = &buf_output_class;
  self->out = wrap;
  self->buf = buffer_new(buffer);
  self->close_backend = true;
  return self;
}

static int64_t buf_output_write(void* _self, const char* src, size_t n) {
  BufOutput* self = _self;
  if (buffer_avail_write(self->buf) == 0) {
    if (!buffer_flush(self->buf, self->out)) return -1;
  }
  return buffer_write(self->buf, src, n);
}

static bool buf_output_put(void* _self, char ch) {
  BufOutput* self = _self;
  if (buffer_avail_write(self->buf) == 0) {
    if (!buffer_flush(self->buf, self->out)) return -1;
  }
  self->buf->data[self->buf->write++] = ch;
  return true;
}

static bool buf_output_flush(void* _self) {
  BufOutput* self = _self;
  if (!buffer_flush(self->buf, self->out)) return false;
  return call(self->out, flush);
}

static void buf_output_close(void* _self) {
  BufOutput* self = _self;
  buf_output_flush(self);
  buffer_free(self->buf);
  if (self->close_backend)
    call(self->out, close);
  free(self);
}

static Output_Class buf_output_class = {
  .write = buf_output_write,
  .put = buf_output_put,
  .flush = buf_output_flush,
  .close = buf_output_close,
};