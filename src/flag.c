
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vlib/flag.h>
#include <vlib/error.h>

void flags_init(Flags* self) {
  hashtable_init(self->flags, hasher_fnv64str, equaler_str, sizeof(const char*), sizeof(Flag));
  self->usage = flag_default_usage;
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

void* add_flag(Flags* self, void* ptr, FlagType* type, const char* name, const char* help) {
  Flag* flag = hashtable_insert(self->flags, &name);
  flag->name = name;
  flag->help = help;
  flag->type = type;
  flag->default_value = v_malloc(type->size);
  flag->ptr = ptr;
  return flag->default_value;
}

bool flags_parse(Flags* self, int argc, char* const argv[], Vector* extra) {

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
          fprintf(stderr, "Illegal value for %s: %s\n", name, value);
          goto Usage;
        }
      } else {
        fprintf(stderr, "Unknown argument: %s\n", name);
        goto Usage;
      }

    } else {
      if (extra) {
        *(char**)vector_push(extra) = arg;
      } else {
        fprintf(stderr, "Extra argument: %s\n", arg);
        goto Usage;
      }
    }
  }
  return true;

Usage:
  self->usage(self, argv[0]);
  return false;
}

void flag_default_usage(Flags* flags, const char* selfname) {
  fprintf(stderr, "Usage: %s\n", selfname);
  if (flags->flags->size > 0) {
    int print_flag(void* _key, void* _data) {
      Flag* flag = _data;
      char valbuf[64];
      flag->type->print(valbuf, flag->default_value);
      char cbuf[512];
      snprintf(cbuf, sizeof(cbuf), "  -%s=%s", flag->name, valbuf);
      fprintf(stderr, "%-20s  %s\n", cbuf, flag->help);
      return HT_CONTINUE;
    }
    hashtable_iter(flags->flags, print_flag);
  }
  exit(1);
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
  snprintf(buf, sizeof(buf), "%s", *(const char**)ptr);
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
  const char* truths[] = {"true", "yes", "on", "1", NULL};
  for (int i = 0; truths[i]; i++) {
    if (strcasecmp(truths[i], src) == 0) {
      *ptr = true;
      return true;
    }
  }
  const char* lies[] = {"false", "no", "off", "0", NULL};
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