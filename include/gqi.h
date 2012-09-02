#ifndef GQI_H_4B3F1BFF797FC8
#define GQI_H_4B3F1BFF797FC8

#include <string.h>

/**
 * General Query Interface
 */

#include <stddef.h>

/* GQI Strings */

typedef struct GQI_String {
  const char* str;
  size_t      sz;
  unsigned    refs;
} GQI_String;

/**
 * GQI_String initialization:
 *
 *    static: Makes no copy of the data and does not free() on close.
 *
 *    copy:   Copies the data and frees it on close.
 *
 *    own:    Makes no copy of the data but claims ownership, ie. the data will be freed when
 *            the string is closed.
 *
 *    null:   Sets the string's data to NULL.
 *
 * After initialization, the string is owned by the caller and should be closed by calling
 * gqis_release().
 */
void gqis_init_static(GQI_String* str, const char* data, size_t sz);
void gqis_init_copy(GQI_String*, const char* data, size_t sz);
void gqis_init_own(GQI_String* str, char* data, size_t sz);
void gqis_init_null(GQI_String* str);

void gqis_acquire(GQI_String* str);
void gqis_release(GQI_String* str);

/* Main interface */

typedef struct GQI_Class {

  /**
   * Perform a General Query given an input string.
   * Returns zero if the operation succeeds, any other value indicates an error.
   * In either case, the resulting string is returned in result and resultsz.
   */
  int (*query)(void* _self, GQI_String* input, GQI_String* result);

  void (*close)(void* _self);

} GQI_Class;

// Base GQI class. All GQI instances should have a struct GQI as the first field, or be
// pointers to a struct GQI (it's the same thing).
typedef struct GQI {
  GQI_Class*  _class;
  int         _refs;
} GQI;

// Main query function. After returning, result will be an initialized GQI_String.
// gqis_release() should be called on the result once it is finished with.
int gqi_query(GQI* instance, GQI_String* input, GQI_String* result);

// Helper function that uses NULL-terminated strings.
// The result should be freed by the caller.
int gqic_query(GQI* instance, const char* query, char** result);

// Utility for GQI implementations.
// After initializing a GQI instance the reference counter will be set to 1, which means
// that gqi_release() must be called at some point in the future.
void gqi_init(void* _instance, GQI_Class* cls);

/**
 * GQI reference count manipulation.
 * A GQI instance will be closed when gqi_release() is called by the last owner.
 * Use gqi_acquire() to become an owner.
 */
void gqi_acquire(GQI* instance);
void gqi_release(GQI* instance);

/**
 * How to implement the GQI* interface:
 *
 * Create a struct with a GQI field in the beginning. For example:
 *    struct My_GQI_Instance {
 *      GQI   _base;
 *      ....
 *    }
 * 
 * Create a GQI_Class value with your implementation's query and close methods.
 * Call gqi_init to initialize an instance of your implementation.
 *
 * Tip: the standard free() function is useful to use for the close() method if your
 * implementation does not allocate any extra memory.
 */

/**
 * Default: always returns a constant string.
 */

GQI* gqi_new_default(const GQI_String* value);

static inline GQI* gqic_new_default(const char* _value) {
  GQI_String value = {_value, strlen(_value)};
  return gqi_new_default(&value);
}

/**
 * GQI Value: returns a constant string if the query string is empty, otherwise it
 * returns NULL. This is intended to be used with GQI_Mux.
 */

GQI* gqi_new_value(const GQI_String* value);

static inline GQI* gqic_new_value(const char* _value) {
  GQI_String value = {_value, strlen(_value)};
  return gqi_new_value(&value);
}

/**
 * Query mux:
 *
 * A GQI_Mux delegates a query to a separate GQI instance based on the first
 * matching prefix string. The prefix is stripped before passing the query on
 * to the sub-instance.
 */

GQI* gqi_new_mux();
void gqi_mux_register(void* mux, const GQI_String* prefix, GQI* delegate);

static inline void gqic_mux_register(void* mux, const char* _prefix, GQI* delegate) {
  GQI_String prefix = {_prefix, strlen(_prefix)};
  gqi_mux_register(mux, &prefix, delegate);
}

/**
 * First: return the first non-NULL result (including errors).
 */
GQI* gqi_new_first();
void gqi_first_add(void* self, GQI* instance);

#endif /* GQI_H_4B3F1BFF797FC8 */

