
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>

#include <vlib/error.h>
#include <vlib/hashtable.h>
#include <vlib/vector.h>

static Hashtable providers[1];
static ErrorProvider general_provider, io_provider;

static Vector try_stack[1];

data(TryFrame) {
  jmp_buf env;
};

void verr_init() {
  hashtable_init(providers, hasher_fnv64, memcmp, sizeof(int), sizeof(ErrorProvider*));
  verr_register(VERR_PGENERAL, &general_provider);
  verr_register(VERR_PIO, &io_provider);

  vector_init(try_stack, sizeof(TryFrame), 3);
}
void verr_cleanup() {
  vector_close(try_stack);
  hashtable_close(providers);
}

void verr_register(int provider, ErrorProvider* impl) {
  if (!hashtable_get(providers, &provider)) {
    *(ErrorProvider**)hashtable_insert(providers, &provider) = impl;
  }
}

const char* verr_msg(error_t err) {
  static char buf[200];

  int provider = VERR_PROVIDER(err);
  void* ptr = hashtable_get(providers, &provider);
  if (!ptr) goto NoDetails;

  ErrorProvider* ep = *(ErrorProvider**)ptr;
  const char* msg = ep->get_msg(err);
  if (!msg) goto NoDetails;
  snprintf(buf, sizeof(buf), "[%s:%x] %s", ep->name, err, msg);
  return buf;

NoDetails:
  snprintf(buf, sizeof(buf), "[unknown:%x]", err);
  return buf;
}

/* General error provider */

static const char* general_get_msg(error_t err) {
  switch (err) {
    case VERR_ARGERR:     return "invalid argument";
    case VERR_MALFORMED:  return "malformed data";
    case VERR_NOMEM:      return "out of memory";
  };
  return NULL;
}

static ErrorProvider general_provider = {
  .name = "general",
  .get_msg = general_get_msg,
};

/* IO provider */

static const char* io_get_msg(error_t err) {
  switch (err) {
    case VERR_IO:   return "general IO error";
    case VERR_EOF:  return "end-of-file";
  }
  return NULL;
}

static ErrorProvider io_provider = {
  .name = "io",
  .get_msg = io_get_msg,
};

/* Exceptions */

void verr_try(void (*action)(), void (*handle)(error_t error), void (*cleanup)()) {
  TryFrame* frame = vector_push(try_stack);
  error_t error = setjmp(frame->env);
  if (error == 0) {
    action();
  } else {
    if (handle) {
      // Wrap the handler in another try frame, so that the cleanup function is still called
      error_t reraise = setjmp(frame->env);
      if (reraise == 0) {
        handle(error);
        error = 0;
      } else {
        error = reraise;
      }
    }
  }
  vector_pop(try_stack);
  if (cleanup) cleanup();
  if (error) verr_raise(error);
}

void verr_raise(error_t error) {
  assert(error != 0);
  if (try_stack->size > 0) {
    TryFrame* frame = vector_back(try_stack);
    longjmp(frame->env, error);
  }
  fprintf(stderr, "unhandled exception: %s\n", verr_msg(error));
  abort();
}
