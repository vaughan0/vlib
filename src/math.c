
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include <vlib/math.h>
#include <vlib/error.h>
#include <vlib/util.h>

double rand_double(RandomSource* source) {
  uint64_t n = call(source, generate);
  uint64_t max = UINT64_MAX;
  return (double)n / ((double)max + 1);
}
int64_t rand_int(RandomSource* source, int64_t min, int64_t max) {
  double range = (double)max - (double)min + 1;
  return (int64_t)(rand_double(source) * range + min);
}
void rand_bytes(RandomSource* source, void* dst, size_t n) {
  uint64_t* ptr = dst;
  while (n >= sizeof(uint64_t)) {
    *ptr = call(source, generate);
    ptr++;
    n -= sizeof(uint64_t);
  }
  if (n > 0) {
    uint64_t num = call(source, generate);
    char* bsrc = (char*)&num;
    char* bdst = (char*)ptr;
    for (; n; n--) *(bdst++) = *(bsrc++);
  }
}

/* Secure random source */

static __thread Input* urandom = NULL;

static uint64_t secure_generate(void* _self) {
  if (!urandom) {
    FILE* fin = fopen("/dev/urandom", "r");
    if (!fin) verr_raise_system();
    urandom = file_input_new(fin, true);
  }
  uint64_t result;
  io_readall(urandom, &result, sizeof(result));
  return result;
}
static RandomSource_Impl secure_impl = {
  .generate = secure_generate,
  .close = null_close,
};

RandomSource secure_random_source[1] = {{
  ._impl = &secure_impl,
}};

void secure_random_cleanup() {
  if (urandom) {
    call(urandom, close);
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

RandomSource* pseudo_random_new(uint64_t seed) {
  PseudoRandom* self = malloc(sizeof(PseudoRandom));
  memset(self, 0, sizeof(*self));
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
  uint64_t num = 0;
  for (unsigned i = 0; i < 4; i++) {
    int32_t r;
    if (random_r(self->buf, &r)) verr_raise_system();
    num = (num << 16) | (r & 0xFFFF);
  }
  return num;
}

static RandomSource_Impl pseudo_impl = {
  .generate = pseudo_generate,
  .close = free,
};
