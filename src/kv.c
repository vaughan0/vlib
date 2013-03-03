
#include <string.h>
#include <stdlib.h>

#include <vlib/kv.h>
#include <vlib/error.h>
#include <vlib/io.h>
#include <vlib/util.h>

#ifdef VLIB_ENABLE_GDBM

#include <gdbm.h>

/* UnclosableOpener */

data(UnclosableOpener) {
  KVOpener  base;
  KVOpener* wrap;
};
static KVOpener_Impl unclosable_impl;

KVOpener* kv_unclosable_opener(KVOpener* wrap) {
  UnclosableOpener* self = malloc(sizeof(UnclosableOpener));
  self->base._impl = &unclosable_impl;
  self->wrap = wrap;
  return &self->base;
}
void kv_unclosable_close(KVOpener* _self) {
  UnclosableOpener* self = (UnclosableOpener*)_self;
  call(self->wrap, close);
  free(self);
}

static KVDB* unclosable_open(void* _self, const char* table) {
  UnclosableOpener* self = _self;
  return call(self->wrap, open, table);
}
static KVDB* unclosable_create(void* _self, const char* table) {
  UnclosableOpener* self = _self;
  return call(self->wrap, create, table);
}
static KVOpener_Impl unclosable_impl = {
  .open = unclosable_open,
  .create = unclosable_create,
  .close = null_close,
};

/* PrefixOpener */

data(PrefixOpener) {
  KVOpener    base;
  KVOpener*   wrap;
  Output*     bufout;
  size_t      offset;
};
static KVOpener_Impl prefix_impl;

KVOpener* kv_prefix_opener(const char* prefix, KVOpener* wrap) {
  PrefixOpener* self = malloc(sizeof(PrefixOpener));
  self->base._impl = &prefix_impl;
  self->bufout = string_output_new(strlen(prefix)+64);
  io_writec(self->bufout, prefix);
  string_output_data(self->bufout, &self->offset);
  return &self->base;
}
void kv_prefix_reset(KVOpener* _self, const char* prefix, KVOpener* wrap) {
  PrefixOpener* self = (PrefixOpener*)_self;
  self->wrap = wrap;
  string_output_reset(self->bufout);
  io_writec(self->bufout, prefix);
  string_output_data(self->bufout, &self->offset);
}

static const char* add_prefix(PrefixOpener* self, const char* table) {
  string_output_rewind(self->bufout, self->offset);
  io_writec(self->bufout, table);
  io_put(self->bufout, '\0');
  return string_output_data(self->bufout, NULL);
}
static KVDB* prefix_open(void* _self, const char* table) {
  PrefixOpener* self = _self;
  return call(self->wrap, open, add_prefix(self, table));
}
static KVDB* prefix_create(void* _self, const char* table) {
  PrefixOpener* self = _self;
  return call(self->wrap, open, add_prefix(self, table));
}
static void prefix_close(void* _self) {
  PrefixOpener* self = _self;
  call(self->bufout, close);
  call(self->wrap, close);
  free(self);
}

static KVOpener_Impl prefix_impl = {
  .open = prefix_open,
  .create = prefix_create,
  .close = prefix_close,
};

/* GDBM Database */

data(GDBM_DB) {
  KVDB      base;
  GDBM_FILE dbf;
};
static KVDB_Impl gdbm_kv_impl;

KVDB* kv_gdbm_open(const char* file, bool create) {
  GDBM_FILE dbf = gdbm_open((char*)file, 0, create ? GDBM_NEWDB : GDBM_WRITER, 0644, NULL);
  if (!dbf) {
    if (gdbm_errno == GDBM_FILE_OPEN_ERROR && create == false) return NULL;
    verr_raise_system();
  }
  GDBM_DB* self = malloc(sizeof(GDBM_DB));
  self->base._impl = &gdbm_kv_impl;
  self->dbf = dbf;
  return &self->base;
}

static void kv_gdbm_lookup(void* _self, KVDatum key, KVDatum* data) {
  GDBM_DB* self = _self;
  datum dbkey = {
    .dptr = key.ptr,
    .dsize = key.size,
  };
  datum result = gdbm_fetch(self->dbf, dbkey);
  if (!result.dptr) {
    data->ptr = NULL;
    return;
  }

  if (data->ptr) {
    if (result.dsize > data->size) {
      free(result.dptr);
      RAISE(ARGUMENT);
    }
    memcpy(data->ptr, result.dptr, result.dsize);
    data->size = result.dsize;
    free(result.dptr);
  } else {
    data->ptr = result.dptr;
    data->size = result.dsize;
  }
}
static void kv_gdbm_store(void* _self, KVDatum key, KVDatum data) {
  GDBM_DB* self = _self;
  datum dbkey = {
    .dptr = key.ptr,
    .dsize = key.size,
  };
  datum dbval = {
    .dptr = data.ptr,
    .dsize = data.size,
  };
  gdbm_store(self->dbf, dbkey, dbval, GDBM_REPLACE);
}
static void kv_gdbm_sync(void* _self) {
  GDBM_DB* self = _self;
  gdbm_sync(self->dbf);
}
static void kv_gdbm_close(void* _self) {
  GDBM_DB* self = _self;
  gdbm_close(self->dbf);
  free(self);
}
static KVDB_Impl gdbm_kv_impl = {
  .lookup = kv_gdbm_lookup,
  .store = kv_gdbm_store,
  .sync = kv_gdbm_sync,
  .close = kv_gdbm_close,
};

/* GDBM Opener */

data(GDBM_Opener) {
  KVOpener  base;
  Output*   buf;
  char*     prefix;
  size_t    prefixlen;
};
static KVOpener_Impl gdbm_opener_impl;

KVOpener* kv_gdbm_opener(const char* prefix) {
  GDBM_Opener* self = malloc(sizeof(GDBM_Opener));
  self->base._impl = &gdbm_opener_impl;
  self->prefixlen = strlen(prefix);
  self->prefix = malloc(self->prefixlen);
  memcpy(self->prefix, prefix, self->prefixlen);
  self->buf = string_output_new(self->prefixlen + 256);
  return &self->base;
}

static KVDB* _gdbm_opener_open(void* _self, const char* table, bool create) {
  GDBM_Opener* self = _self;

  string_output_reset(self->buf);
  io_write(self->buf, self->prefix, self->prefixlen);
  io_writec(self->buf, table);
  io_writelit(self->buf, ".db\0");
  const char* file = string_output_data(self->buf, NULL);

  return kv_gdbm_open(file, create);
}
static KVDB* gdbm_opener_open(void* self, const char* table) {
  return _gdbm_opener_open(self, table, false);
}
static KVDB* gdbm_opener_create(void* self, const char* table) {
  return _gdbm_opener_open(self, table, true);
}
static void gdbm_opener_close(void* _self) {
  GDBM_Opener* self = _self;
  free(self->prefix);
  call(self->buf, close);
  free(self);
}
static KVOpener_Impl gdbm_opener_impl = {
  .open = gdbm_opener_open,
  .create = gdbm_opener_create,
  .close = gdbm_opener_close,
};

#endif
