
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <vlib/math.h>
#include <vlib/error.h>

double rand_double(RandomSource* source) {
  uint64_t n = call(source, generate);
  uint64_t max = ~((uint64_t)0);
  return (double)n / ((double)max + 1);
}
int rand_int(RandomSource* source, int min, int max) {
  int range = max - min + 1;
  return rand_double(source) * range + min;
}

/* Secure random source */

static __thread FILE* urandom = NULL;

static size_t secure_read(void* _self, char* dst, size_t n) {
  if (!urandom) {
    urandom = fopen("/dev/random", "r");
    if (!urandom) verr_raise_system();
  }
  size_t nread = fread(dst, 1, n, urandom);
  if (!nread) {
    if (feof(urandom))
      RAISE(EOF);
    else
      RAISE(IO);
  }
  return nread;
}
static bool secure_eof(void* _self) {
  return false;
}
static Input_Impl secure_impl = {
  .read = secure_read,
  .eof = secure_eof,
};

Input secure_random_source[1] = {{
  ._impl = &secure_impl,
}};

void secure_random_cleanup() {
  if (urandom) {
    fclose(urandom);
    urandom = NULL;
  }
}

/* Pseudo-random source */

data(PseudoRandom) {
  RandomSource        base;
  struct random_data  buf[1];
  char                state[256];
};

static RandomSource_Impl pseudo_impl;

RandomSource* pseudo_random_new(Input* seed_source) {
  unsigned int seed;
  io_readall(seed_source, &seed, sizeof(seed));

  PseudoRandom* self = malloc(sizeof(PseudoRandom));
  self->base._impl = &pseudo_impl;
  if (initstate_r(seed, self->state, sizeof(self->state), self->buf)) {
    int eno = errno;
    free(self);
    verr_raise(verr_system(eno));
  }
  
  return &self->base;
}

static uint64_t pseudo_generate(void* _self) {
  PseudoRandom* self = _self;
  union {
    int32_t   arr[2];
    uint64_t  result;
  } u;
  if (random_r(self->buf, &u.arr[0])) verr_raise_system();
  if (random_r(self->buf, &u.arr[1])) verr_raise_system();
  return u.result;
}

static RandomSource_Impl pseudo_impl = {
  .generate = pseudo_generate,
  .close = free,
};
