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

data(BufInput) {
  Input     base;
  Input*    in;
  Buffer*   buf;
  bool      close_backend;
};

void* buf_input_new(Input* wrap, size_t buffer);

data(BufOutput) {
  Output      base;
  Output*     out;
  Buffer*     buf;
  bool        close_backend;
};

void* buf_output_new(Output* wrap, size_t buffer);

#endif /* BUFIO_H_CC4072C147916A */

