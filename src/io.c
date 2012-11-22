
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <vlib/util.h>
#include <vlib/io.h>
#include <vlib/vector.h>

/* Base functions */

int64_t io_read(Input* in, char* dst, size_t n) {
  if (in->_class->read) {
    return call(in, read, dst, n);
  }
  assert(in->_class->get);
  unsigned r = 0;
  while (r < n) {
    int ch = call(in, get);
    if (ch == -1) break;
    dst[r++] = ch;
  }
  return r;
}
int io_get(Input* in) {
  if (in->_class->get) {
    return call(in, get);
  }
  assert(in->_class->read);
  char ch;
  return call(in, read, &ch, 1) ? ch : -1;
}
void io_unget(Input* in) {
  assert(in->_class->unget);
  call(in, unget);
}

int64_t io_write(Output* out, const char* src, size_t n) {
  if (out->_class->write) {
    return call(out, write, src, n);
  }
  assert(out->_class->put);
  unsigned w = 0;
  while (w < n) {
    if (call(out, put, src[w++])) break;
  }
  return w;
}
int64_t io_write_full(Output* out, const char* src, size_t n) {
  size_t written = 0;
  while (written < n) {
    int64_t r = io_write(out, src+written, n-written);
    if (r <= 0) break;
    written += r;
  }
  return written;
}
bool io_put(Output* out, char ch) {
  if (out->_class->put) {
    return call(out, put, ch);
  }
  assert(out->_class->write);
  return io_write(out, &ch, 1) == 1;
}
bool io_flush(Output* output) {
  assert(output->_class->flush);
  return call(output, flush);
}

/* Utilities */

int64_t io_copy(Input* from, Output* to) {
  char cbuf[4096];
  int64_t copied = 0;
  for (;;) {
    int64_t r = io_read(from, cbuf, sizeof(cbuf));
    if (r <= 0) break;
    int64_t w = io_write_full(to, cbuf, r);
    copied += w;
    if (w != r) break;
  }
  return copied;
}

/* StringInput */

data(StringInput) {
  Input    base;
  size_t      size;
  size_t      offset;
  const char* src;
};

static Input_Class string_input_class;

Input* string_input_new(const char* src, size_t sz) {
  StringInput* self = malloc(sizeof(StringInput));
  self->base._class = &string_input_class;
  self->size = sz;
  self->offset = 0;
  self->src = src;
  return &self->base;
}

static int64_t string_input_read(void* _self, char* dst, size_t n) {
  StringInput* self = _self;
  n = MIN(n, self->size - self->offset);
  memcpy(dst, self->src + self->offset, n);
  self->offset += n;
  return n;
}

static int string_input_get(void* _self) {
  StringInput* self = _self;
  if (self->offset < self->size) {
    return self->src[self->offset++];
  }
  return -1;
}

static void string_input_unget(void* _self) {
  StringInput* self = _self;
  assert(self->offset > 0);
  self->offset--;
}

static Input_Class string_input_class = {
  .read = string_input_read,
  .get = string_input_get,
  .unget = string_input_unget,
  .close = free,
};

/* StringOutput */

data(Piece) {
  Piece*      next;
  size_t      size;
  char        data[0];
};

data(StringOutput) {
  Output   base;
  size_t      offset;
  Piece*      first;
  Piece*      last;
};

static Output_Class string_output_class;

Output* string_output_new(size_t initcap) {
  StringOutput* self = malloc(sizeof(StringOutput));
  self->base._class = &string_output_class;
  self->offset = 0;
  self->first = malloc(sizeof(Piece) + initcap);
  self->first->next = NULL;
  self->first->size = initcap;
  self->last = self->first;
  return &self->base;
}

static void make_piece(StringOutput* self) {
  size_t sz = self->last->size * 2;
  Piece* newpiece = malloc(sizeof(Piece) + sz);
  newpiece->size = sz;
  newpiece->next = NULL;
  self->offset = 0;
  self->last->next = newpiece;
  self->last = newpiece;
}

static int64_t string_output_write(void* _self, const char* src, size_t n) {
  StringOutput* self = _self;

  size_t fill = MIN(n, self->last->size - self->offset);
  memcpy(self->last->data + self->offset, src, fill);

  if (fill == n) {
    self->offset += n;
    return n;
  } else {
    make_piece(self);
    return fill + string_output_write(self, src + fill, n - fill);
  }
}

static bool string_output_put(void* _self, char ch) {
  StringOutput* self = _self;
  if (self->offset == self->last->size) {
    make_piece(self);
  }
  self->last->data[self->offset++] = ch;
  return true;
}

static bool string_output_flush(void* self) {
  return true;
}

static void string_output_close(void* _self) {
  StringOutput* self = _self;
  Piece *p, *tmp;
  for (p = self->first; p; p = tmp) {
    tmp = p->next;
    free(p);
  }
  free(self);
}

const char* string_output_data(void* _self, size_t* store_size) {
  StringOutput* self = _self;

  if (self->first->next) {

    /* Collapse pieces into one */

    size_t total = 0;
    for (Piece* p = self->first; p; p = p->next) {
      total += p->size;
    }

    Piece* bigone = malloc(sizeof(Piece) + total);
    bigone->size = total;
    bigone->next = NULL;
    
    size_t offset = 0;
    Piece* tmp;
    for (Piece* p = self->first; p->next; p = tmp) {
      tmp = p->next;
      memcpy(bigone->data + offset, p->data, p->size);
      offset += p->size;
      free(p);
    }
    memcpy(bigone->data + offset, self->last->data, self->offset);
    offset += self->offset;
    free(self->last);

    self->first = self->last = bigone;
    self->offset = offset;

  }

  Piece* p = self->first;
  if (store_size) {
    *store_size = self->offset;
  }
  return p->data;
}

static Output_Class string_output_class = {
  .write = string_output_write,
  .put = string_output_put,
  .flush = string_output_flush,
  .close = string_output_close,
};

/* File descriptor wrappers */

data(FDInput) {
  Input   base;
  FILE*   file;
};

static Input_Class fd_input_class;

Input* fd_input_new(int fd) {
  FILE* file = fdopen(fd, "r");
  if (!file) return NULL;
  FDInput* self = malloc(sizeof(FDInput));
  self->base._class = &fd_input_class;
  self->file = file;
  return &self->base;
}

static int64_t fd_input_read(void* _self, char* dst, size_t n) {
  FDInput* self = _self;
  return fread(dst, 1, n, self->file);
}

static int fd_input_get(void* _self) {
  FDInput* self = _self;
  return fgetc(self->file);
}

static void fd_input_close(void* _self) {
  FDInput* self = _self;
  fclose(self->file);
  free(self);
}

static Input_Class fd_input_class = {
  .read = fd_input_read,
  .get = fd_input_get,
  .close = fd_input_close,
};


data(FDOutput) {
  Output  base;
  FILE*   file;
};

static Output_Class fd_output_class;

Output* fd_output_new(int fd) {
  FILE* file = fdopen(fd, "w");
  if (!file) return NULL;
  FDOutput* self = malloc(sizeof(FDOutput));
  self->base._class = &fd_output_class;
  self->file = file;
  return &self->base;
}

static int64_t fd_output_write(void* _self, const char* src, size_t n) {
  FDOutput* self = _self;
  return fwrite(src, 1, n, self->file);
}

static bool fd_output_put(void* _self, char ch) {
  FDOutput* self = _self;
  return fputc((int)ch, self->file) != EOF;
}

static bool fd_output_flush(void* _self) {
  FDOutput* self = _self;
  return fflush(self->file) == 0;
}

static void fd_output_close(void* _self) {
  FDOutput* self = _self;
  fclose(self->file);
  free(self);
}

static Output_Class fd_output_class = {
  .write = fd_output_write,
  .put = fd_output_put,
  .flush = fd_output_flush,
  .close = fd_output_close,
};
