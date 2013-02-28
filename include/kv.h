#ifndef KV_H_3EF446161DB48B
#define KV_H_3EF446161DB48B

#include <vlib/std.h>

data(KVDatum) {
  void*   ptr;
  size_t  size;
};

interface(KVDB) {
  void    (*lookup)(void* self, KVDatum key, KVDatum* data);
  void    (*store)(void* self, KVDatum key, KVDatum data);
  void    (*close)(void* self);
};

#endif /* KV_H_3EF446161DB48B */

