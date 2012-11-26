#ifndef ERROR_H_0CCB7725EEA9D5
#define ERROR_H_0CCB7725EEA9D5

#include <stdint.h>
#include <stdbool.h>

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

/* Standard providers */

enum {
  VERR_PGENERAL,
  VERR_PIO,
};

// General error codes
enum {
  VERR_ARGERR     = VERR_MAKE(VERR_PGENERAL, 1),
  VERR_MALFORMED  = VERR_MAKE(VERR_PGENERAL, 2),
  VERR_NOMEM      = VERR_MAKE(VERR_PGENERAL, 3),
};

// IO error codes
enum {
  VERR_IO   = VERR_MAKE(VERR_PIO, 1),  // general IO error
  VERR_EOF  = VERR_MAKE(VERR_PIO, 2),
};

/* Exception handling */

void    verr_try(void (*action)(), void (*handle)(error_t error), void (*cleanup)());
void    verr_raise(error_t error);

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
