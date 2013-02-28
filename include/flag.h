#ifndef FLAG_H_D36A1C1C6AFA5C
#define FLAG_H_D36A1C1C6AFA5C

#include <stdbool.h>

#include <vlib/std.h>
#include <vlib/hashtable.h>
#include <vlib/vector.h>
#include <vlib/io.h>

struct Flags;

data(FlagType) {
  size_t  size;
  bool    (*parse)(const char* src, void* ptr);    
  void    (*print)(char buf[64], void* ptr);
};

typedef void (*UsageFunc)(struct Flags* flags, const char* error);

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
  const char* name;
};

void  flags_init(Flags* self);
void  flags_close(Flags* self);

void  print_usage(Flags* self, Output* out);
void  print_flags(Flags* self, Output* out);
void  flags_error(Flags* self, const char* error);
void  flags_errorf(Flags* self, const char* fmt, ...);

void  add_flag(Flags* self, void* ptr, FlagType* type, const char* name, const char* help);
bool  flags_parse(Flags* self, int argc, char* const argv[], Vector* extra);

/* Flag types */

extern FlagType flagtype_int[1];
extern FlagType flagtype_string[1];
extern FlagType flagtype_bool[1];

#endif /* FLAG_H_D36A1C1C6AFA5C */

