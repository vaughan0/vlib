#ifndef KV_H_3EF446161DB48B
#define KV_H_3EF446161DB48B

#include <assert.h>

#include <vlib/std.h>

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
  void    (*lookup)(void* self, Bytes key, Bytes* data);
  void    (*store)(void* self, Bytes key, Bytes data);
  void    (*sync)(void* self);
  void    (*close)(void* self);
};

interface(KVOpener) {
  // Opens and returns an existing table. Returns NULL if the table does not exist.
  KVDB*   (*open)(void* self, const char* table);
  // Creates a new table, replacing an existing table if there is one.
  KVDB*   (*create)(void* self, const char* table);
  void    (*close)(void* self);
};

static inline KVDB* kv_open_create(KVOpener* opener, const char* table) {
  KVDB* db = call(opener, open, table);
  if (!db) db = call(opener, create, table);
  assert(db);
  return db;
}

KVOpener* kv_unclosable_opener(KVOpener* wrap);
void      kv_unclosable_close(KVOpener* self);

KVOpener* kv_prefix_opener(const char* prefix, KVOpener* wrap);
void      kv_prefix_reset(KVOpener* self, const char* prefix, KVOpener* wrap);

#ifdef VLIB_ENABLE_GDBM

KVDB*     kv_gdbm_open(const char* file, bool create);
KVOpener* kv_gdbm_opener(const char* prefix);

#endif

#endif /* KV_H_3EF446161DB48B */

