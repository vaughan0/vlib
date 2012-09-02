
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vlib/gqi.h>

/* GQI_String */

void gqis_init_static(GQI_String* str, const char* data, size_t sz) {
  str->str = (char*)data;
  str->sz = sz;
  // add an extra reference to prevent the data from ever being freed
  str->refs = 2;
}
void gqis_init_copy(GQI_String* str, const char* data, size_t sz) {
  str->str = malloc(sz);
  memcpy((char*)str->str, data, sz);
  str->sz = sz;
  str->refs = 1;
}
void gqis_init_own(GQI_String* str, char* data, size_t sz) {
  str->str = data;
  str->sz = sz;
  str->refs = 1;
}
void gqis_init_null(GQI_String* str) {
  str->str = NULL;
  str->sz = 0;
  str->refs = 2;
}

void gqis_acquire(GQI_String* str) {
  str->refs++;
}
void gqis_release(GQI_String* str) {
  assert(str->refs > 0);
  str->refs--;
  if (str->refs == 0) {
    if (str->str) free((char*)str->str);
  }
}

/* Main functions */

int gqi_query(GQI* instance, GQI_String* input, GQI_String* result) {
  gqis_acquire(input);
  int r = instance->_class->query(instance, input, result);
  if (r) assert(result->str);
  gqis_release(input);
  return r;
}
int gqic_query(GQI* instance, const char* input, char** result) {
  GQI_String ginput;
  gqis_init_static(&ginput, input, strlen(input));
  GQI_String gresult;
  int r = gqi_query(instance, &ginput, &gresult);

  if (gresult.str) {
    *result = malloc(gresult.sz+1);
    memcpy(*result, gresult.str, gresult.sz);
    (*result)[gresult.sz] = 0;
  } else {
    *result = NULL;
  }
  gqis_release(&gresult);

  return r;
}

void gqi_acquire(GQI* instance) {
  // Make sure the instance has not been closed already
  assert(instance->_refs != 0);
  instance->_refs++;
}
void gqi_release(GQI* instance) {
  assert(instance->_refs > 0);
  instance->_refs--;
  if (instance->_refs == 0) {
    instance->_class->close(instance);
  }
}

void gqi_init(void* _instance, GQI_Class* cls) {
  GQI* instance = _instance;
  instance->_class = cls;
  instance->_refs = 1;
}

/* GQI_Value (Default and Value) */

typedef struct GQI_Value {
  GQI     _base;
  size_t  sz;
  char    value[0];
} GQI_Value;

static int default_query(void* _self, GQI_String* input, GQI_String* result) {
  GQI_Value* self = _self;
  gqis_init_static(result, self->value, self->sz);
  return 0;
}

static GQI_Class default_class = {
  .query = default_query,
  .close = free,
};

GQI* gqi_new_default(const GQI_String* value) {
  GQI_Value* self = malloc(sizeof(GQI_Value) + value->sz);
  gqi_init(self, &default_class);
  self->sz = value->sz;
  memcpy(self->value, value->str, value->sz);
  return (GQI*)self;
}

static int value_query(void* _self, GQI_String* input, GQI_String* result) {
  GQI_Value* self = _self;
  if (input->sz == 0) {
    gqis_init_static(result, self->value, self->sz);
  } else {
    gqis_init_null(result);
  }
  return 0;
}

static GQI_Class value_class = {
  .query = value_query,
  .close = free,
};

GQI* gqi_new_value(const GQI_String* value) {
  GQI_Value* self = malloc(sizeof(GQI_Value) + value->sz);
  gqi_init(self, &value_class);
  self->sz = value->sz;
  memcpy(self->value, value->str, value->sz);
  return (GQI*)self;
}

/* GQI_Mux */

typedef struct MuxEntry {
  struct MuxEntry* next;
  GQI*    delegate;
  size_t  len;
  char    prefix[0];
} MuxEntry;

typedef struct GQI_Mux {
  GQI       _base;
  MuxEntry* first;
  MuxEntry* last;
} GQI_Mux;

static int mux_query(void* _self, GQI_String* input, GQI_String* result) {
  GQI_Mux* self = _self;
  for (MuxEntry* entry = self->first; entry; entry = entry->next) {
    if (entry->len > input->sz) continue;
    if (memcmp(input->str, entry->prefix, entry->len) == 0) {
      GQI_String stripped;
      gqis_init_static(&stripped,
          input->str + entry->len,
          input->sz - entry->len);
      return gqi_query(entry->delegate, &stripped, result);
    }
  }
  result->str = NULL;
  return 0;
}
static void mux_close(void* _self) {
  GQI_Mux* self = _self;
  MuxEntry *entry, *tmp;
  for (entry = self->first; entry; entry = tmp) {
    tmp = entry->next;
    gqi_release(entry->delegate);
    free(entry);
  }
  free(_self);
}

static GQI_Class mux_class = {
  .query = mux_query,
  .close = mux_close,
};

GQI* gqi_new_mux() {
  GQI_Mux* self = malloc(sizeof(GQI_Mux));
  gqi_init(self, &mux_class);
  self->first = NULL;
  self->last = NULL;
  return (GQI*)self;
}
void gqi_mux_register(void* _self, const GQI_String* prefix, GQI* delegate) {
  GQI_Mux* self = _self;

  MuxEntry* entry = malloc(sizeof(MuxEntry) + prefix->sz);
  entry->next = NULL;
  entry->delegate = delegate;
  gqi_acquire(delegate);
  entry->len = prefix->sz;
  memcpy(entry->prefix, prefix->str, prefix->sz);

  if (self->last) {
    self->last->next = entry;
  } else {
    self->first = entry;
  }
  self->last = entry;
}

/* GQI_First */

typedef struct FirstEntry {
  struct FirstEntry* next;
  GQI* instance;
} FirstEntry;

typedef struct GQI_First {
  GQI           _base;
  FirstEntry*   root;
  FirstEntry*   last;
} GQI_First;

static int first_query(void* _self, GQI_String* input, GQI_String* result) {
  GQI_First* self = _self;
  for (FirstEntry* entry = self->root; entry; entry = entry->next) {
    int r = gqi_query(entry->instance, input, result);
    if (r) {
      // error
      return r;
    } else if (result->str) {
      return 0;
    }
  }
  gqis_init_null(result);
  return 0;
}
static void first_close(void* _self) {
  GQI_First* self = _self;
  FirstEntry *entry, *tmp;
  for (entry = self->root; entry; entry = tmp) {
    tmp = entry->next;
    gqi_release(entry->instance);
    free(entry);
  }
  free(self);
}

static GQI_Class first_class = {
  .query = first_query,
  .close = first_close,
};

GQI* gqi_new_first() {
  GQI_First* self = malloc(sizeof(GQI_First));
  gqi_init(self, &first_class);
  self->root = NULL;
  self->last = NULL;
  return (GQI*)self;
}
void gqi_first_add(void* _self, GQI* instance) {
  GQI_First* self = _self;

  FirstEntry* entry = malloc(sizeof(FirstEntry));
  entry->next = NULL;
  entry->instance = instance;
  gqi_acquire(instance);

  if (!self->last) {
    self->root = entry;
    self->last = entry;
  } else {
    self->last->next = entry;
  }
}
