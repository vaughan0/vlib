#ifndef MATH_H_CDD63B14FF71E7
#define MATH_H_CDD63B14FF71E7

#include <vlib/std.h>
#include <vlib/io.h>

interface(RandomSource) {
  uint64_t  (*generate)(void* self);
  void      (*close)(void* self);
};

double      rand_double(RandomSource* source);
int         rand_int(RandomSource* source, int min, int max);

RandomSource* pseudo_random_new(Input* seed_source);

extern Input  secure_random_source[1];
void          secure_random_cleanup() __attribute__((destructor));

#endif /* MATH_H_CDD63B14FF71E7 */

