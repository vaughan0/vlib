
#include <string.h>
#include <stdlib.h>

#include <vlib/kv.h>
#include <vlib/error.h>

#ifdef VLIB_ENABLE_GDBM

#include <gdbm.h>

data(GDBM_DB) {
  KVDB      base;
  GDBM_FILE dbf;
};
static KVDB_Impl gdbm_kv_impl;

KVDB* kv_open_gdbm(const char* file) {
  GDBM_FILE dbf = gdbm_open((char*)file, 0, GDBM_WRCREAT, 0644, NULL);
  if (!dbf) verr_raise_system();
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

#endif
