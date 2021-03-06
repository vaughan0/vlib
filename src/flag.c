
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <vlib/flag.h>
#include <vlib/error.h>

static void flag_default_usage(Flags* self, const char* error);

void flags_init(Flags* self) {
  hashtable_init(self->flags, hasher_fnv64str, equaler_str, sizeof(const char*), sizeof(Flag));
  self->usage = flag_default_usage;
  self->name = NULL;
}
void flags_close(Flags* self) {
  int free_flag(void* _key, void* _data) {
    Flag* flag = _data;
    if (flag->default_value) free(flag->default_value);
    return HT_CONTINUE;
  }
  hashtable_iter(self->flags, free_flag);
  hashtable_close(self->flags);
}

void add_flag(Flags* self, FlagType* type, const char* name, void* ptr, const char* help) {
  Flag* flag = hashtable_insert(self->flags, &name);
  flag->name = name;
  flag->help = help;
  flag->type = type;
  flag->default_value = malloc(type->size);
  memcpy(flag->default_value, ptr, type->size);
  flag->ptr = ptr;
}

bool flags_parse(Flags* self, int argc, char* const argv[], Vector* extra) {
  char error[256];
  if (!self->name) self->name = argv[0];

  // Initialize all flags to their default values
  int init_flag(void* _key, void* _data) {
    Flag* flag = _data;
    memcpy(flag->ptr, flag->default_value, flag->type->size);
    return HT_CONTINUE;
  }
  hashtable_iter(self->flags, init_flag);

  for (int i = 1; i < argc; i++) {
    char* arg = argv[i];
    if (*arg == '-') {

      if (*(++arg) == '-') arg++; // allow double-dash
      char* name = arg;
      char* value = strchr(arg, '=');
      if (value) *(value++) = 0;
      Flag* flag = hashtable_get(self->flags, &name);
      if (flag) {
        if (!flag->type->parse(value, flag->ptr)) {
          snprintf(error, sizeof(error), "Illegal value for %s: %s", name, value);
          goto Usage;
        }
      } else {
        snprintf(error, sizeof(error), "Unknown argument: %s", name);
        goto Usage;
      }

    } else {
      if (extra) {
        *(char**)vector_push(extra) = arg;
      } else {
        snprintf(error, sizeof(error), "Unexpected extra argument: %s", arg);
        goto Usage;
      }
    }
  }
  return true;

Usage:
  if (self->usage) self->usage(self, error);
  return false;
}

void print_flags(Flags* self, Output* out) {
  int print_flag(void* _key, void* _data) {
    Flag* flag = _data;
    // Format flag name and value
    char valbuf[64];
    flag->type->print(valbuf, flag->default_value);
    char cbuf[512];
    int n = snprintf(cbuf, sizeof(cbuf), "  -%s=%s", flag->name, valbuf);
    // Print nicely aligned line
    io_write(out, cbuf, n);
    for (int i = n; i < 20; i++) io_put(out, ' ');
    io_writelit(out, "  ");
    io_writec(out, flag->help);
    io_put(out, '\n');
    return HT_CONTINUE;
  }
  hashtable_iter(self->flags, print_flag);
}
void print_usage(Flags* self, Output* out) {
  io_writelit(out, "Usage: ");
  io_writec(out, self->name);
  io_put(out, '\n');
  print_flags(self, out);
}

static void flag_default_usage(Flags* flags, const char* error) {
  Output* out = buf_output_new(file_output_new(stderr, false), 1024);
  io_writec(out, error);
  io_put(out, '\n');
  print_usage(flags, out);
  call(out, close);
  exit(1);
}

void flags_error(Flags* self, const char* error) {
  self->usage(self, error);
}
void flags_errorf(Flags* self, const char* fmt, ...) {
  char error[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(error, sizeof(error), fmt, ap);
  va_end(ap);
  flags_error(self, error);
}

/* Flag types */

static bool int_parse(const char* src, void* ptr) {
  if (!src) return false;
  errno = 0;
  char* endptr;
  long int value = strtol(src, &endptr, 10);
  if (errno != 0 || endptr == src) return false;
  *(int*)ptr = value;
  return true;
}
static void int_print(char buf[64], void* ptr) {
  snprintf(buf, sizeof(buf), "%d", *(int*)ptr);
}
FlagType flagtype_int[1] = {{
  .size = sizeof(int),
  .parse = int_parse,
  .print = int_print,
}};

static bool string_parse(const char* src, void* ptr) {
  if (!src) return false;
  *(const char**)ptr = src;
  return true;
}
static void string_print(char buf[64], void* ptr) {
  snprintf(buf, sizeof(buf), "%s", *(const char**)ptr ?: "(null)");
}
FlagType flagtype_string[1] = {{
  .size = sizeof(const char*),
  .parse = string_parse,
  .print = string_print,
}};

static bool bool_parse(const char* src, void* _ptr) {
  bool* ptr = _ptr;
  if (!src) {
    *ptr = true;
    return true;
  }
  const char* truths[] = {"true", "y", "yes", "on", "1", NULL};
  for (int i = 0; truths[i]; i++) {
    if (strcasecmp(truths[i], src) == 0) {
      *ptr = true;
      return true;
    }
  }
  const char* lies[] = {"false", "n", "no", "off", "0", NULL};
  for (int i = 0; lies[i]; i++) {
    if (strcasecmp(lies[i], src) == 0) {
      *ptr = false;
      return true;
    }
  }
  return false;
}
static void bool_print(char buf[64], void* ptr) {
  snprintf(buf, sizeof(buf), "%s", *(bool*)ptr ? "true" : "false");
}
FlagType flagtype_bool[1] = {{
  .size = sizeof(bool),
  .parse = bool_parse,
  .print = bool_print,
}};
