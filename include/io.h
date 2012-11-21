#ifndef IO_H_2A5A68C6C96499
#define IO_H_2A5A68C6C96499

#include <stdint.h>
#include <stdbool.h>

#include <vlib/std.h>

interface(RawInput) {
  int64_t (*read)(void* self, char* dst, size_t n);
  void    (*close)(void* self);
};

interface(RawOutput) {
  int64_t (*write)(void* self, const char* src, size_t n);
  void    (*flush)(void* self);
  void    (*close)(void* self);
};

#endif /* IO_H_2A5A68C6C96499 */

