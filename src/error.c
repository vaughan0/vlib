
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
static __thread const char* current_msg;

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
  static __thread char buf[200];
  int n = 0;

  void write(const char* fmt, ...) {
    if (n < sizeof(buf)) {
      va_list va;
      va_start(va, fmt);
      n += vsnprintf(buf+n, sizeof(buf)-n, fmt, va);
      va_end(va);
    }
  }

  write("[%x:", err);

  int provider = VERR_PROVIDER(err);
  void* ptr = hashtable_get(providers, &provider);
  if (!ptr) {
    write("unknown]");
  } else {
    ErrorProvider* ep = *(ErrorProvider**)ptr;
    write("%s]", ep->name);
    const char* msg = ep->get_msg(err);
    if (msg) {
      write(" %s", msg);
    }
  }

  return buf;
}
const char* verr_current_msg() {
  return current_msg;
}
const char* verr_current_str() {
  static __thread char buf[512];
  int n = snprintf(buf, sizeof(buf), "%s", verr_msg(current_error));
  if (n < sizeof(buf) && current_msg) {
    snprintf(buf+n, sizeof(buf)-n, " - %s", current_msg);
  }
  return buf;
}

/* General error provider */

static const char* general_get_msg(error_t err) {
  switch (err) {
  case VERR_ARGUMENT:     return "invalid argument";
  case VERR_MALFORMED:    return "malformed data";
  case VERR_NOMEM:        return "out of memory";
  case VERR_ACCESS:       return "permission denied";
  case VERR_SYSTEM:       return "system error";
  case VERR_INTERRUPT:    return "operation interrupted";
  case VERR_STATE:        return "illegal state";
  case VERR_TIMEOUT:      return "timeout";
  case VERR_UNAVAILABLE:  return "resource unavailable";
  case VERR_REMOTE:       return "remote error";
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
  case EAGAIN:
    return VERR_UNAVAILABLE;
  case ETIMEDOUT:
    return VERR_TIMEOUT;
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
static __thread char error_msg[512];
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

void verr_raisef(error_t error, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(error_msg, sizeof(error_msg), fmt, ap);
  va_end(ap);
  verr_raise_msg(error, error_msg);
}
void verr_raise(error_t error) {
  verr_raise_msg(error, NULL);
}
void verr_raise_msg(error_t error, const char* msg) {
  assert(error != 0);
#ifdef DEBUG
  backtrace_size = backtrace(backtrace_buffer, sizeof(backtrace_buffer) / sizeof(void*));
#endif
  current_error = error;
  current_msg = msg;
  verr_reraise();
}
void verr_reraise() {
  if (try_stack->size > 0) {
    TryFrame* frame = vector_back(try_stack);
    longjmp(frame->env, current_error);
  }
  fprintf(stderr, "unhandled exception: %s", verr_msg(current_error));
  if (current_msg) {
    fprintf(stderr, " - %s\n", current_msg);
  } else {
    fprintf(stderr, "\n");
  }
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
