
#include <string.h>
#include <stdlib.h>

#include <vlib/kv.h>
#include <vlib/error.h>
#include <vlib/io.h>

#ifdef VLIB_ENABLE_GDBM

#include <gdbm.h>

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
static void kv_gdbm_close(void* _self) {
  GDBM_DB* self = _self;
  gdbm_close(self->dbf);
  free(self);
}
static KVDB_Impl gdbm_kv_impl = {
  .lookup = kv_gdbm_lookup,
  .store = kv_gdbm_store,
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
