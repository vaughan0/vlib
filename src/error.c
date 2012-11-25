
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <vlib/error.h>
#include <vlib/hashtable.h>

static Hashtable providers[1];
static ErrorProvider general_provider, io_provider;

void verr_init() {
  hashtable_init(providers, hasher_fnv64, memcmp, sizeof(int), sizeof(ErrorProvider*));
  verr_register(VERR_PGENERAL, &general_provider);
  verr_register(VERR_PIO, &io_provider);
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
  snprintf(buf, sizeof(buf), "[%s] %s", ep->name, msg);
  return buf;

NoDetails:
  snprintf(buf, sizeof(buf), "error code: %x", err);
  return buf;
}

/* General error provider */

static const char* general_get_msg(error_t err) {
  switch (err) {
    case VERR_ARGERR:     return "invalid argument";
    case VERR_MALFORMED:  return "malformed data";
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
