
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <execinfo.h>

#include <vlib/error.h>
#include <vlib/hashtable.h>
#include <vlib/vector.h>

static bool initialized = false;
static Hashtable providers[1];
static ErrorProvider general_provider, io_provider;

static __thread Vector try_stack[1];
static __thread error_t current_error;

data(TryFrame) {
  jmp_buf env;
};

void verr_init() {
  if (initialized) return;
  initialized = true;

  hashtable_init(providers, hasher_fnv64, memcmp, sizeof(int), sizeof(ErrorProvider*));
  verr_register(VERR_PGENERAL, &general_provider);
  verr_register(VERR_PIO, &io_provider);
  verr_thread_init();
}
void verr_cleanup() {
  verr_thread_cleanup();
  hashtable_close(providers);
}

void verr_thread_init() {
  vector_init(try_stack, sizeof(TryFrame), 3);
}
void verr_thread_cleanup() {
  vector_close(try_stack);
}

void verr_register(int provider, ErrorProvider* impl) {
  verr_init();
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
  snprintf(buf, sizeof(buf), "[%x:%s] %s", err, ep->name, msg);
  return buf;

NoDetails:
  snprintf(buf, sizeof(buf), "[%x:unknown]", err);
  return buf;
}

/* General error provider */

static const char* general_get_msg(error_t err) {
  switch (err) {
  case VERR_ARGUMENT:   return "invalid argument";
  case VERR_MALFORMED:  return "malformed data";
  case VERR_NOMEM:      return "out of memory";
  case VERR_ACCESS:     return "permission denied";
  case VERR_SYSTEM:     return "system error";
  case VERR_INTERRUPT:  return "operation interrupted";
  case VERR_STATE:      return "illegal state";
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

error_t verr_system(int eno) {
  switch (eno) {
  case EACCES:
    return VERR_ACCESS;
  case EINTR:
    return VERR_INTERRUPT;
  case ENOBUFS:
  case ENOMEM:
    return VERR_NOMEM;
  case EFBIG:
  case EIO:
  case ENOSPC:
  case EPIPE:
    return VERR_IO;
  default:
    return VERR_SYSTEM;
  }
}

/* Exceptions */

#ifdef DEBUG
static __thread void* backtrace_buffer[32];
static __thread int backtrace_size;
#endif

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
#ifdef DEBUG
  backtrace_size = backtrace(backtrace_buffer, sizeof(backtrace_buffer) / sizeof(void*));
#endif
  current_error = error;
  verr_reraise();
}
void verr_reraise() {
  if (try_stack->size > 0) {
    TryFrame* frame = vector_back(try_stack);
    longjmp(frame->env, current_error);
  }
  fprintf(stderr, "unhandled exception: %s\n", verr_msg(current_error));
#ifdef DEBUG
  verr_print_stacktrace();
#endif
  abort();
}

void verr_print_stacktrace() {
#ifdef DEBUG
  char** symbols = backtrace_symbols(backtrace_buffer, backtrace_size);
  if (!symbols) {
    backtrace_symbols_fd(backtrace_buffer, backtrace_size, 2);
    return;
  }
  for (int i = 0; i < backtrace_size; i++) {
    fprintf(stderr, "  %s\n", symbols[i]);
  }
  free(symbols);
#endif
}
