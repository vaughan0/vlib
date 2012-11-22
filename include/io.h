#ifndef IO_H_2A5A68C6C96499
#define IO_H_2A5A68C6C96499

#include <stdint.h>
#include <stdbool.h>

#include <vlib/std.h>

interface(Input) {
  int64_t (*read)(void* self, char* dst, size_t n);
  int     (*get)(void* self);
  void    (*unget)(void* self);
  void    (*close)(void* self);
};

interface(Output) {
  int64_t (*write)(void* self, const char* src, size_t n);
  bool    (*put)(void* self, char ch);
  bool    (*flush)(void* self);
  void    (*close)(void* self);
};

int64_t     io_read(Input* input, char* dst, size_t n);
int         io_get(Input* input);
void        io_unget(Input* input);

int64_t     io_write(Output* output, const char* src, size_t n);
bool        io_put(Output* output, char ch);
bool        io_flush(Output* output);

/* Some standard IO implementations */

Input*      string_input_new(const char* src, size_t sz);

Output*     string_output_new(size_t initcap);
const char* string_output_data(void* string_output, size_t* size);

#endif /* IO_H_2A5A68C6C96499 */

