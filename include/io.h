#ifndef IO_H_2A5A68C6C96499
#define IO_H_2A5A68C6C96499

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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

void        io_readall(Input* input, void* dst, size_t sz);

static inline void io_writec(Output* out, const char* str) {
  io_write(out, str, strlen(str));
}
#define io_writelit(out, str) io_write((out), (str), sizeof(str)-1);

void        io_put_varint(Output* out, int64_t i);
void        io_put_uvarint(Output* out, uint64_t u);

void        io_put_int8(Output* out, int8_t i);
void        io_put_int16(Output* out, int16_t i);
void        io_put_int32(Output* out, int32_t i);
void        io_put_int64(Output* out, int64_t i);

int64_t     io_get_varint(Input* in);
uint64_t    io_get_uvarint(Input* in);

int8_t      io_get_int8(Input* in);
int16_t     io_get_int16(Input* in);
int32_t     io_get_int32(Input* in);
int64_t     io_get_int64(Input* in);

/* Formatting */

data(IOFormatter) {
  Output*   out;
  char*     _buffer;
  size_t    _bufsz;
};

void        io_format_init(IOFormatter* self, Output* out, size_t initcap);
void        io_format_close(IOFormatter* self);

void        io_format(IOFormatter* self, const char* fmt, ...);
void        io_vformat(IOFormatter* self, const char* fmt, va_list ap);

/* Some standard IO implementations */

Input*      buf_input_new(Input* wrap, size_t buffer_size);
void        buf_input_reset(Input* self, Input* wrap);

Output*     buf_output_new(Output* wrap, size_t buffer_size);
void        buf_output_reset(Output* self, Output* wrap);

Output*     string_output_new(size_t initcap);
const char* string_output_data(Output* string_output, size_t* size);
void        string_output_rewind(Output* string_output, size_t new_offset);
void        string_output_reset(Output* string_output);

Input*      memory_input_new(const char* src, size_t sz);
void        memory_input_reset(Input* memory_input, const char* src, size_t sz);

Output*     memory_output_new(void* dst, size_t sz);
void        memory_output_reset(Output* memory_output, void* dst, size_t sz);
void        memory_output_rewind(Output* memory_output, size_t new_offset);
size_t      memory_output_size(Output* memory_output);

Input*      file_input_new(FILE* f, bool close);
Output*     file_output_new(FILE* f, bool close);

Input*      fd_input_new(int fd, bool close);
Output*     fd_output_new(int fd, bool close);

Input*      limited_input_new(Input* in, size_t limit);

Input*      unclosable_input_new(Input* wrap);
void        unclosable_input_close(Input* self);
Output*     unclosable_output_new(Output* wrap);
void        unclosable_output_close(Output* self);

extern Input  null_input;   // Always returns EOF
extern Output null_output;  // Discards all data
extern Input  zero_input;   // Always reads zeroes

#endif /* IO_H_2A5A68C6C96499 */

