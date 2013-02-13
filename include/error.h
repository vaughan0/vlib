#ifndef ERROR_H_0CCB7725EEA9D5
#define ERROR_H_0CCB7725EEA9D5

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include <vlib/std.h>

/* Error codes:
 * b00000cc cccccccc pppppppp pppppppp
 * b:   Bad bit - 0 for success, 1 for error
 * c:   Error code
 * p:   Provider Id
 */

#define VERR_CODE(err) (((err) & 0x03FF0000) >> 16)
#define VERR_PROVIDER(err) ((err) & 0x0000FFFF)
#define VERR_MAKE(provider, code) ((1 << 31) | (((code) & 0x3FF) << 16) | ((provider) & 0xFFFF))

typedef int32_t error_t;

const char* verr_msg(error_t eno);

/* Pluggable providers */

data(ErrorProvider) {
  const char* name;
  const char* (*get_msg)(error_t code);
};

void verr_register(int provider, ErrorProvider* impl);

void verr_init() __attribute__((constructor));
void verr_cleanup() __attribute__((destructor));

void verr_thread_init();
void verr_thread_cleanup();

/* Standard providers */

enum {
  VERR_PGENERAL,
  VERR_PIO,
  VERR_PNET,
};

// General error codes
enum {
  VERR_ARGUMENT   = VERR_MAKE(VERR_PGENERAL, 1),  // invalid argument
  VERR_MALFORMED  = VERR_MAKE(VERR_PGENERAL, 2),  // malformed data
  VERR_NOMEM      = VERR_MAKE(VERR_PGENERAL, 3),  // out of memory
  VERR_ACCESS     = VERR_MAKE(VERR_PGENERAL, 4),  // permission denied
  VERR_SYSTEM     = VERR_MAKE(VERR_PGENERAL, 5),  // system error (generally from a system call)
  VERR_INTERRUPT  = VERR_MAKE(VERR_PGENERAL, 6),  // operation interrupted
  VERR_STATE      = VERR_MAKE(VERR_PGENERAL, 7),  // illegal state
};

// IO error codes
enum {
  VERR_IO   = VERR_MAKE(VERR_PIO, 1),  // general IO error
  VERR_EOF  = VERR_MAKE(VERR_PIO, 2),
};

// Returns the most suitable error code for the given system error number
error_t verr_system(int eno);

/* Exception handling */

void    verr_try(void (*action)(), void (*handle)(error_t error), void (*cleanup)());
void    verr_raise(error_t error);
// Reraise an exception from a CATCH block
void    verr_reraise();

static inline void verr_raise_system() {
  verr_raise(verr_system(errno));
}

void    verr_print_stacktrace();

#define RAISE(verr) verr_raise(VERR_##verr)

// (There be dragons ahead)
#define TRY { void (*_action)(); void (*_handle)(error_t) = NULL; void (*_cleanup)() = NULL; \
  _action = ({ void _func()
#define CATCH(err) _func;}); _handle = ({ void _func(error_t err)
#define FINALLY _func;}); _cleanup = ({ void _func()
#define ETRY _func;}); verr_try(_action, _handle, _cleanup); }

// Safe memory allocation
static inline void* v_malloc(size_t sz) {
  void* ptr = malloc(sz);
  if (!ptr) verr_raise(VERR_NOMEM);
  return ptr;
}
static inline void* v_realloc(void* ptr, size_t sz) {
  ptr = realloc(ptr, sz);
  if (!ptr && sz != 0) verr_raise(VERR_NOMEM);
  return ptr;
}

#endif /* ERROR_H_0CCB7725EEA9D5 */
