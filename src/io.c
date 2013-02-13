
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <unistd.h>

#include <vlib/util.h>
#include <vlib/io.h>
#include <vlib/vector.h>

static void close_free(void* _self) {
  free(_self);
}

/* Base functions */

size_t io_read(Input* in, char* dst, size_t n) {
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
  if (n == 0) return;
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

size_t io_copy(Input* from, Output* to) {
  char cbuf[4096];
  size_t copied = 0;
  for (;;) {
    size_t r = io_read(from, cbuf, sizeof(cbuf));
    if (r == 0) break;
    io_write(to, cbuf, r);
    copied += r;
  }
  return copied;
}
size_t io_copyn(Input* from, Output* to, size_t n) {
  char cbuf[4096];
  size_t copied = 0;
  while (copied < n) {
    size_t r = io_read(from, cbuf, MIN(sizeof(cbuf), n-copied));
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

static size_t string_input_read(void* _self, char* dst, size_t n) {
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
static bool string_input_eof(void* _self) {
  StringInput* self = _self;
  return self->offset == self->size;
}

static Input_Impl string_input_impl = {
  .read = string_input_read,
  .get = string_input_get,
  .unget = string_input_unget,
  .eof = string_input_eof,
  .close = close_free,
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
  self->last->data[self->offset++] = ch;
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

/* Limited input */

data(LimitedInput) {
  Input   base;
  Input*  in;
  size_t  limit;
  size_t  read;
};

static size_t limited_read(void* _self, char* dst, size_t n) {
  LimitedInput* self = _self;
  n = MIN(n, self->limit - self->read);
  if (n == 0) return 0;
  size_t r = io_read(self->in, dst, n);
  self->read += r;
  return r;
}
static int limited_get(void* _self) {
  LimitedInput* self = _self;
  if (self->read < self->limit) {
    int ch = io_get(self->in);
    self->read += (ch == -1) ? 0 : 1;
    return ch;
  } else {
    return -1;
  }
}
static void limited_unget(void* _self) {
  LimitedInput* self = _self;
  self->read--;
  io_unget(self->in);
}
static bool limited_eof(void* _self) {
  LimitedInput* self = _self;
  return (self->read == self->limit) || io_eof(self->in);
}
static void limited_close(void* _self) {
  LimitedInput* self = _self;
  call(self->in, close);
  free(self);
}

static Input_Impl limited_impl = {
  .read = limited_read,
  .get = limited_get,
  .unget = limited_unget,
  .eof = limited_eof,
  .close = limited_close,
};
Input* limited_input_new(Input* in, size_t limit) {
  LimitedInput* self = v_malloc(sizeof(LimitedInput));
  self->base._impl = &limited_impl;
  self->in = in;
  self->limit = limit;
  self->read = 0;
  return (Input*)self;
}

/* File wrappers */

data(FileInput) {
  Input   base;
  FILE*   file;
  bool    close;
};

static Input_Impl file_input_impl;

Input* file_input_new(FILE* file, bool close) {
  FileInput* self = v_malloc(sizeof(FileInput));
  self->base._impl = &file_input_impl;
  self->file = file;
  self->close = close;
  return &self->base;
}
static size_t file_input_read(void* _self, char* dst, size_t n) {
  FileInput* self = _self;
  if (feof(self->file)) return 0;
  return fread(dst, 1, n, self->file);
}
static int file_input_get(void* _self) {
  FileInput* self = _self;
  int c = fgetc(self->file);
  return (c == EOF) ? -1 : c;
}
static bool file_input_eof(void* _self) {
  FileInput* self = _self;
  return feof(self->file) != 0;
}
static void file_input_close(void* _self) {
  FileInput* self = _self;
  if (self->close) fclose(self->file);
  free(self);
}

static Input_Impl file_input_impl = {
  .read = file_input_read,
  .get = file_input_get,
  .eof = file_input_eof,
  .close = file_input_close,
};

data(FileOutput) {
  Output  base;
  FILE*   file;
  bool    close;
};

static Output_Impl file_output_impl;

Output* file_output_new(FILE* file, bool close) {
  FileOutput* self = v_malloc(sizeof(FileOutput));
  self->base._impl = &file_output_impl;
  self->file = file;
  self->close = close;
  return &self->base;
}
static void file_output_write(void* _self, const char* src, size_t n) {
  FileOutput* self = _self;
  if (fwrite(src, 1, n, self->file) != n) RAISE(IO);
}
static void file_output_put(void* _self, char ch) {
  FileOutput* self = _self;
  if (fputc((int)ch, self->file) == EOF) verr_raise(VERR_IO);
}
static void file_output_flush(void* _self) {
  FileOutput* self = _self;
  if (fflush(self->file)) verr_raise(VERR_IO);
}
static void file_output_close(void* _self) {
  FileOutput* self = _self;
  if (self->close) fclose(self->file);
  free(self);
}

static Output_Impl file_output_impl = {
  .write = file_output_write,
  .put = file_output_put,
  .flush = file_output_flush,
  .close = file_output_close,
};

/* File descriptor wrappers */

data(FDInput) {
  Input base;
  int   fd;
  bool  eof;
  bool  close;
};

static Input_Impl fd_input_impl;

Input* fd_input_new(int fd, bool close) {
  FDInput* self = v_malloc(sizeof(FDInput));
  self->base._impl = &fd_input_impl;
  self->fd = fd;
  self->eof = false;
  self->close = close;
  return &self->base;
}

static size_t fd_input_read(void* _self, char* dst, size_t n) {
  FDInput* self = _self;
  if (self->eof) return 0;
  ssize_t nread = read(self->fd, dst, n);
  if (nread == -1) verr_raise(VERR_SYSTEM);
  if (nread == 0) self->eof = true;
  return nread;
}
static bool fd_input_eof(void* _self) {
  FDInput* self = _self;
  return self->eof;
}
static void fd_input_close(void* _self) {
  FDInput* self = _self;
  if (self->close) close(self->fd);
  free(self);
}

static Input_Impl fd_input_impl = {
  .read = fd_input_read,
  .eof = fd_input_eof,
  .close = fd_input_close,
};

data(FDOutput) {
  Output  base;
  int     fd;
  bool    close;
};

static Output_Impl fd_output_impl;

Output* fd_output_new(int fd, bool close) {
  FDOutput* self = v_malloc(sizeof(FDOutput));
  self->base._impl = &fd_output_impl;
  self->fd = fd;
  self->close = close;
  return &self->base;
}

static void fd_output_write(void* _self, const char* src, size_t n) {
  FDOutput* self = _self;
  while (n) {
    ssize_t written = write(self->fd, src, n);
    if (written == -1) verr_raise_system();
    n -= written;
    src += written;
  }
}
static void fd_output_flush(void* _self) { /* unbuffered */ }
static void fd_output_close(void* _self) {
  FDOutput* self = _self;
  if (self->close) close(self->fd);
  free(self);
}

static Output_Impl fd_output_impl = {
  .write = fd_output_write,
  .flush = fd_output_flush,
  .close = fd_output_close,
};

/* Unclosable IO */

data(UnclosableInput) {
  Input   base;
  Input*  wrap;
};

static size_t unclosable_read(void* _self, char* dst, size_t n) {
  UnclosableInput* self = _self;
  return io_read(self->wrap, dst, n);
}
static int unclosable_get(void* _self) {
  UnclosableInput* self = _self;
  return io_get(self->wrap);
}
static void unclosable_unget(void* _self) {
  UnclosableInput* self = _self;
  return io_unget(self->wrap);
}
static bool unclosable_eof(void* _self) {
  UnclosableInput* self = _self;
  return io_eof(self->wrap);
}
static void _unclosable_input_close(void* _self) {
  /* I'm UNCLOSABLE!!!1!1!!111!!! */
}

static Input_Impl unclosable_input_impl = {
  .read = unclosable_read,
  .get = unclosable_get,
  .unget = unclosable_unget,
  .eof = unclosable_eof,
  .close = _unclosable_input_close,
};

Input* unclosable_input_new(Input* wrap) {
  UnclosableInput* self = v_malloc(sizeof(UnclosableInput));
  self->base._impl = &unclosable_input_impl;
  self->wrap = wrap;
  return &self->base;
}
void unclosable_input_close(Input* _self) {
  UnclosableInput* self = (UnclosableInput*)_self;
  call(self->wrap, close);
  free(self);
}

data(UnclosableOutput) {
  Output  base;
  Output* wrap;
};

static void unclosable_write(void* _self, const char* src, size_t n) {
  UnclosableOutput* self = _self;
  io_write(self->wrap, src, n);
}
static void unclosable_put(void* _self, char ch) {
  UnclosableOutput* self = _self;
  io_put(self->wrap, ch);
}
static void unclosable_flush(void* _self) {
  UnclosableOutput* self = _self;
  io_flush(self->wrap);
}
static void _unclosable_output_close(void* _self) {
  /* unclosable etc etc */
}

static Output_Impl unclosable_output_impl = {
  .write = unclosable_write,
  .put = unclosable_put,
  .flush = unclosable_flush,
  .close = _unclosable_output_close,
};

Output* unclosable_output_new(Output* wrap) {
  UnclosableOutput* self = v_malloc(sizeof(UnclosableOutput));
  self->base._impl = &unclosable_output_impl;
  self->wrap = wrap;
  return &self->base;
}
void unclosable_output_close(Output* _self) {
  UnclosableOutput* self = (UnclosableOutput*)_self;
  call(self->wrap, close);
  free(self);
}

/* Null IO */

static size_t null_input_read(void* self, char* dst, size_t n) {
  return 0;
}
static int null_input_get(void* self) {
  return -1;
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

static size_t zero_input_read(void* self, char* dst, size_t n) {
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
