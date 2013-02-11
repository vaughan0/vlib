#ifndef FLAG_H_D36A1C1C6AFA5C
#define FLAG_H_D36A1C1C6AFA5C

#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/hashtable.h>
#include <vlib/vector.h>

struct Flags;

data(FlagType) {
  size_t  size;
  bool    (*parse)(const char* src, void* ptr);    
  void    (*print)(char buf[64], void* ptr);
};

typedef void (*UsageFunc)(struct Flags* flags, const char* selfname);

void  flag_default_usage(struct Flags* flags, const char* selfname);

data(Flag) {
  const char* name;
  const char* help;
  FlagType*   type;
  void*       default_value;
  void*       ptr;
};

data(Flags) {
  UsageFunc   usage;
  Hashtable   flags[1];
};

void  flags_init(Flags* self);
void  flags_close(Flags* self);

// Adds a flag and returns a pointer to its default value.
void* add_flag(Flags* self, void* ptr, FlagType* type, const char* name, const char* help);

bool  flags_parse(Flags* self, int argc, char* const argv[], Vector* extra);

/* Flag types */

extern FlagType flagtype_int[1];
extern FlagType flagtype_string[1];
extern FlagType flagtype_bool[1];

#endif /* FLAG_H_D36A1C1C6AFA5C */

