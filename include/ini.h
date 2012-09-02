#ifndef INI_H_A089732CABEE2D
#define INI_H_A089732CABEE2D

#include <stdbool.h>

#include <vlib/hashtable.h>
#include <vlib/gqi.h>

typedef struct INI_Node {
  struct INI_Node* next;
  char* name;
  void* data;
} INI_Node;

typedef struct INI {
  INI_Node* root;
} INI;

void ini_init(INI* self);
void ini_close(INI* self);
void ini_clear(INI* self);

// Returns the value for the given key or NULL if no such value was found
const char* ini_lookup(INI* self, const char* section, const char* key);

void ini_add(INI* self, const char* section, const char* key, const char* value);

// Parse an INI file
int ini_parse(INI* self, const char* text, const char** err);

// Generate an INI file
void ini_dump(INI* self, char* buf, size_t bufsz);

// GQI implementation. If free_on_close is non-zero then the INI object
// will be freed when the GQI instance is closed.
GQI* gqi_new_ini(INI* ini, bool free_on_close);

#endif /* INI_H_A089732CABEE2D */

