#ifndef KV_H_3EF446161DB48B
#define KV_H_3EF446161DB48B

#include <vlib/std.h>

data(KVDatum) {
  void*   ptr;
  size_t  size;
};

interface(KVDB) {
  /** Performs a lookup.
   * If data->ptr is NULL, then data->size will be ignored and a new block
   * of memory will be allocated and stored in data->ptr to hold the result of the operation.
   *
   * If data->ptr is not NULL, then the result will be stored in the given memory location. In
   * this case, if the data is greater than data->size, a VERR_ARGUMENT exception will be raised.
   *
   * If the lookup fails, data->ptr will be set to NULL. If it succeeds, then data->size will be
   * set to the size of the result data.
   */
  void    (*lookup)(void* self, KVDatum key, KVDatum* data);
  void    (*store)(void* self, KVDatum key, KVDatum data);
  void    (*close)(void* self);
};

interface(KVOpener) {
  KVDB*   (*open)(void* self, const char* table);
  void    (*close)(void* self);
};

#ifdef VLIB_ENABLE_GDBM

KVDB* kv_open_gdbm(const char* file);

#endif

#endif /* KV_H_3EF446161DB48B */

