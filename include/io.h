#ifndef IO_H_2A5A68C6C96499
#define IO_H_2A5A68C6C96499

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <vlib/std.h>
#include <vlib/error.h>

interface(Input) {
  size_t  (*read)(void* self, char* dst, size_t n);
  int     (*get)(void* self);
  void    (*unget)(void* self);
  bool    (*eof)(void* self);
  void    (*close)(void* self);
};

interface(Output) {
  void    (*write)(void* self, const char* src, size_t n);
  void    (*put)(void* self, char ch);
  void    (*flush)(void* self);
  void    (*close)(void* self);
};

size_t      io_read(Input* input, char* dst, size_t n);
int         io_get(Input* input);
void        io_unget(Input* input);
bool        io_eof(Input* input);

void        io_write(Output* output, const char* src, size_t n);
void        io_put(Output* output, char ch);
void        io_flush(Output* output);

/* Utility functions */

size_t      io_copy(Input* from, Output* to);
size_t      io_copyn(Input* from, Output* to, size_t max);

/* Some standard IO implementations */

Input*      string_input_new(const char* src, size_t sz);

Output*     string_output_new(size_t initcap);
const char* string_output_data(Output* string_output, size_t* size);
void        string_output_reset(Output* string_output);

Input*      file_input_new(FILE* f);
Output*     file_output_new(FILE* f);

Input*      limited_input_new(Input* in, size_t limit);

extern Input  null_input;   // Always returns EOF
extern Output null_output;  // Discards all data
extern Input  zero_input;   // Always reads zeroes

#endif /* IO_H_2A5A68C6C96499 */

