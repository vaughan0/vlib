
#include <stdlib.h>
#include <string.h>

#include <vlib/bufio.h>

/* Input */

static RawInput_Class input_class;

Input* input_new(RawInput* wrap, size_t buffer) {
  Input* self = malloc(sizeof(Input));
  self->base._class = &input_class;
  self->in = wrap;
  self->buf = buffer_new(buffer);
  self->close_backend = true;
  return self;
}

static void input_close(void* _self) {
  Input* self = _self;
  buffer_free(self->buf);
  if (self->close_backend)
    call(self->in, close);
  free(self);
}

static int64_t input_read(void* _self, char* dst, size_t n) {
  Input* self = _self;
  if (buffer_avail_read(self->buf) == 0) {
    buffer_fill(self->buf, self->in);
  }
  return buffer_read(self->buf, dst, n);
}

int input_get(Input* self) {
  if (!buffer_avail_read(self->buf)) {
    buffer_fill(self->buf, self->in);
    if (!buffer_avail_read(self->buf)) {
      return -1;
    }
  }
  return self->buf->data[self->buf->read++];
}

void input_unget(Input* self) {
  self->buf->read--;
}

static RawInput_Class input_class = {
  .read = input_read,
  .close = input_close,
};

/* Output */

static RawOutput_Class output_class;

Output* output_new(RawOutput* wrap, size_t buffer) {
  Output* self = malloc(sizeof(Output));
  self->base._class = &output_class;
  self->out = wrap;
  self->buf = buffer_new(buffer);
  self->close_backend = true;
  return self;
}

static int64_t output_write(void* _self, const char* src, size_t n) {
  Output* self = _self;
  if (buffer_avail_write(self->buf) == 0) {
    buffer_flush(self->buf, self->out);
  }
  return buffer_write(self->buf, src, n);
}

static void output_flush(void* _self) {
  Output* self = _self;
  buffer_flush(self->buf, self->out);
  call(self->out, flush);
}

static void output_close(void* _self) {
  Output* self = _self;
  output_flush(self);
  buffer_free(self->buf);
  if (self->close_backend)
    call(self->out, close);
  free(self);
}

static RawOutput_Class output_class = {
  .write = output_write,
  .flush = output_flush,
  .close = output_close,
};
