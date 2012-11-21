#ifndef BUFIO_H_CC4072C147916A
#define BUFIO_H_CC4072C147916A

#include <stddef.h>
#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/buffer.h>

/** Buffered IO
 *
 * The Input and Output classes implement RawInput and RawOutput, respectively, by
 * wrapping other IO instances. Their main purpose is to provide simple buffering capabilities.
 *
 * The close_backend attribute is used to control whether the backend instance
 * is closed along with the containing instance or not. It defaults to true, so be careful.
 */

data(Input) {
  RawInput  base;
  RawInput* in;
  Buffer*   buf;
  bool      close_backend;
};

Input*  input_new(RawInput* wrap, size_t buffer);

int     input_get(Input* self);
void    input_unget(Input* self);

data(Output) {
  RawOutput   base;
  RawOutput*  out;
  Buffer*     buf;
  bool        close_backend;
};

Output* output_new(RawOutput* wrap, size_t buffer);

void    output_put(Output* self, char ch);

#endif /* BUFIO_H_CC4072C147916A */

