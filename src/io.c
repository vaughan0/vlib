
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <vlib/util.h>
#include <vlib/io.h>
#include <vlib/vector.h>

/* Base functions */

int64_t io_read(Input* in, char* dst, size_t n) {
  if (in->_impl->read) {
    return call(in, read, dst, n);
  }
  assert(in->_impl->get);
  unsigned r = 0;
  while (r < n) {
    int ch = call(in, get);
    if (ch < 0) break;
    dst[r++] = ch;
  }
  return r;
}
int io_get(Input* in) {
  if (in->_impl->get) {
    return call(in, get);
  }
  assert(in->_impl->read);
  char ch;
  return call(in, read, &ch, 1) ? ch : -1;
}
void io_unget(Input* in) {
  assert(in->_impl->unget);
  call(in, unget);
}
bool io_eof(Input* in) {
  if (in->_impl->eof) {
    return call(in, eof);
  }
  assert(in->_impl->unget);
  int c = io_get(in);
  io_unget(in);
  return c == -1;
}

void io_write(Output* out, const char* src, size_t n) {
  if (out->_impl->write) {
    call(out, write, src, n);
    return;
  }
  assert(out->_impl->put);
  for (unsigned i = 0; i < n; i++) {
    call(out, put, src[i]);
  }
}
void io_put(Output* out, char ch) {
  if (out->_impl->put) {
    call(out, put, ch);
    return;
  }
  assert(out->_impl->write);
  io_write(out, &ch, 1);
}
void io_flush(Output* output) {
  assert(output->_impl->flush);
  call(output, flush);
}

/* Utilities */

int64_t io_copy(Input* from, Output* to) {
  char cbuf[4096];
  int64_t copied = 0;
  for (;;) {
    int64_t r = io_read(from, cbuf, sizeof(cbuf));
    if (r <= 0) break;
    io_write(to, cbuf, r);
    copied += r;
  }
  return copied;
}
int64_t io_copyn(Input* from, Output* to, size_t n) {
  char cbuf[4096];
  size_t copied = 0;
  while (copied < n) {
    int64_t r = io_read(from, cbuf, MIN(sizeof(cbuf), n-copied));
    if (r <= 0) break;
    io_write(to, cbuf, r);
    copied += r;
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

static Input_Impl string_input_impl;

Input* string_input_new(const char* src, size_t sz) {
  StringInput* self = v_malloc(sizeof(StringInput));
  self->base._impl = &string_input_impl;
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
  return VERR_EOF;
}

static void string_input_unget(void* _self) {
  StringInput* self = _self;
  assert(self->offset > 0);
  self->offset--;
}

static bool string_input_eof(void* _self) {
  StringInput* self = _self;
  return self->offset == self->size;
}

static Input_Impl string_input_impl = {
  .read = string_input_read,
  .get = string_input_get,
  .unget = string_input_unget,
  .eof = string_input_eof,
  .close = free,
};

/* StringOutput */

data(Piece) {
  Piece*      next;
  size_t      size;
  char        data[0];
};

data(StringOutput) {
  Output      base;
  size_t      offset;
  Piece*      first;
  Piece*      last;
};

static Output_Impl string_output_impl;

Output* string_output_new(size_t initcap) {
  StringOutput* self = v_malloc(sizeof(StringOutput));
  self->base._impl = &string_output_impl;
  self->offset = 0;
  self->first = malloc(sizeof(Piece) + initcap);
  if (!self->first) {
    free(self);
    verr_raise(VERR_NOMEM);
  }
  self->first->next = NULL;
  self->first->size = initcap;
  self->last = self->first;
  return &self->base;
}

static void make_piece(StringOutput* self) {
  size_t sz = self->last->size * 2;
  Piece* newpiece = v_malloc(sizeof(Piece) + sz);
  newpiece->size = sz;
  newpiece->next = NULL;
  self->offset = 0;
  self->last->next = newpiece;
  self->last = newpiece;
}

static void string_output_write(void* _self, const char* src, size_t n) {
  StringOutput* self = _self;

  size_t fill = MIN(n, self->last->size - self->offset);
  memcpy(self->last->data + self->offset, src, fill);

  if (fill == n) {
    self->offset += n;
  } else {
    make_piece(self);
    string_output_write(self, src + fill, n - fill);
  }
}

static void string_output_put(void* _self, char ch) {
  StringOutput* self = _self;
  if (self->offset == self->last->size) {
    make_piece(self);
  }
}

static void string_output_flush(void* self) { }

static void string_output_close(void* _self) {
  StringOutput* self = _self;
  Piece *p, *tmp;
  for (p = self->first; p; p = tmp) {
    tmp = p->next;
    free(p);
  }
  free(self);
}

const char* string_output_data(Output* _self, size_t* store_size) {
  StringOutput* self = (StringOutput*)_self;

  if (self->first->next) {

    /* Collapse pieces into one */

    size_t total = 0;
    for (Piece* p = self->first; p; p = p->next) {
      total += p->size;
    }

    Piece* bigone = v_malloc(sizeof(Piece) + total);
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

void string_output_reset(Output* _self) {
  StringOutput* self = (StringOutput*)_self;

  // Free all pieces except the last
  Piece* tmp;
  for (Piece* p = self->first; p != self->last; p = tmp) {
    tmp = p->next;
    free(p);
  }

  self->first = self->last;
  self->offset = 0;
}

static Output_Impl string_output_impl = {
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

static Input_Impl fd_input_impl;

Input* fd_input_new(int fd) {
  FILE* file = fdopen(fd, "r");
  if (!file) return NULL;
  FDInput* self = v_malloc(sizeof(FDInput));
  self->base._impl = &fd_input_impl;
  self->file = file;
  return &self->base;
}

static int64_t fd_input_read(void* _self, char* dst, size_t n) {
  FDInput* self = _self;
  if (feof(self->file)) return 0;
  return fread(dst, 1, n, self->file);
}

static int fd_input_get(void* _self) {
  FDInput* self = _self;
  int c = fgetc(self->file);
  return (c == EOF) ? VERR_EOF : c;
}

static bool fd_input_eof(void* _self) {
  FDInput* self = _self;
  return feof(self->file) != 0;
}

static void fd_input_close(void* _self) {
  FDInput* self = _self;
  fclose(self->file);
  free(self);
}

static Input_Impl fd_input_impl = {
  .read = fd_input_read,
  .get = fd_input_get,
  .eof = fd_input_eof,
  .close = fd_input_close,
};

data(FDOutput) {
  Output  base;
  FILE*   file;
};

static Output_Impl fd_output_impl;

Output* fd_output_new(int fd) {
  FDOutput* self = v_malloc(sizeof(FDOutput));
  FILE* file = fdopen(fd, "w");
  if (!file) {
    free(self);
    return NULL;
  }
  self->base._impl = &fd_output_impl;
  self->file = file;
  return &self->base;
}

static void fd_output_write(void* _self, const char* src, size_t n) {
  FDOutput* self = _self;
  if (fwrite(src, 1, n, self->file) != n) RAISE(IO);
}

static void fd_output_put(void* _self, char ch) {
  FDOutput* self = _self;
  if (fputc((int)ch, self->file) == EOF) verr_raise(VERR_IO);
}

static void fd_output_flush(void* _self) {
  FDOutput* self = _self;
  if (fflush(self->file)) verr_raise(VERR_IO);
}

static void fd_output_close(void* _self) {
  FDOutput* self = _self;
  fclose(self->file);
  free(self);
}

static Output_Impl fd_output_impl = {
  .write = fd_output_write,
  .put = fd_output_put,
  .flush = fd_output_flush,
  .close = fd_output_close,
};

/* Null IO */

static int64_t null_input_read(void* self, char* dst, size_t n) {
  return 0;
}
static int null_input_get(void* self) {
  return VERR_EOF;
}
static void null_input_unget(void* self) {}
static void null_input_close(void* self) {}
static bool null_input_eof(void* self) {return true;}

static Input_Impl null_input_impl = {
  .read = null_input_read,
  .get = null_input_get,
  .unget = null_input_unget,
  .eof = null_input_eof,
  .close = null_input_close,
};

Input null_input = {
  ._impl = &null_input_impl,
};

static void null_output_write(void* self, const char* src, size_t n) {}
static void null_output_put(void* self, char ch) {}
static void null_output_flush(void* self) {}
static void null_output_close(void* self) {}

static Output_Impl null_output_impl = {
  .write = null_output_write,
  .put = null_output_put,
  .flush = null_output_flush,
  .close = null_output_close,
};

Output null_output = {
  ._impl = &null_output_impl,
};

/* Zero input */

static int64_t zero_input_read(void* self, char* dst, size_t n) {
  memset(dst, 0, n);
  return n;
}
static int zero_input_get(void* self) {
  return 0;
}
static void zero_input_unget(void* self) {}
static void zero_input_close(void* self) {}
static bool zero_input_eof(void* self) {return false;}

static Input_Impl zero_input_impl = {
  .read = zero_input_read,
  .get = zero_input_get,
  .unget = zero_input_unget,
  .eof = zero_input_eof,
  .close = zero_input_close,
};

Input zero_input = {
  ._impl = &zero_input_impl,
};
