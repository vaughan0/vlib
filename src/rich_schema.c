
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rich_schema.h>
#include <vlib/hashtable.h>
#include <vlib/bitset.h>
#include <vlib/util.h>

data(BoundSource) {
  rich_Source   base;
  rich_Schema*  schema;
  void*         from;
};
static rich_Source_Impl bound_source_impl;

rich_Source* rich_bind_source(rich_Schema* schema, void* from) {
  BoundSource* self = malloc(sizeof(BoundSource));
  self->base._impl = &bound_source_impl;
  self->schema = schema;
  self->from = from;
  return &self->base;
}

static void bound_read_value(void* _self, rich_Sink* to) {
  BoundSource* self = _self;
  call(self->schema, dump_value, self->from, to);
}
static void bound_source_close(void* _self) {
  BoundSource* self = _self;
  call(self->schema, close);
  free(self);
}
static rich_Source_Impl bound_source_impl = {
  .read_value = bound_read_value,
  .close = bound_source_close,
};

data(BoundSink) {
  rich_Sink     base;
  rich_Schema*  schema;
  void*         to;
  Coroutine     co[1];
};
static rich_Sink_Impl bound_sink_impl;

static co_State sink_root_state;

rich_Sink* rich_bind_sink(rich_Schema* schema, void* to) {
  BoundSink* self = malloc(sizeof(BoundSink));
  self->base._impl = &bound_sink_impl;
  self->schema = schema;
  self->to = to;
  coroutine_init(self->co);
  *(BoundSink**)coroutine_push(self->co, &sink_root_state, sizeof(BoundSink*)) = self;
  return &self->base;
}
static void bound_sink_sink(void* _self, rich_Atom atom, void* atom_data) {
  BoundSink* self = _self;
  rich_SchemaArg arg = {
    .atom = atom,
    .data = atom_data,
  };
  coroutine_run(self->co, &arg);
}
static void bound_sink_close(void* _self) {
  BoundSink* self = _self;
  coroutine_close(self->co);
  free(self);
}
static rich_Sink_Impl bound_sink_impl = {
  .sink = bound_sink_sink,
  .close = bound_sink_close,
};

static void root_state_run(void* udata, Coroutine* co, void* arg) {
  BoundSink* self = *(BoundSink**)udata;
  call(self->schema, push_state, co, self->to);
  coroutine_run(self->co, arg);
}
static co_State sink_root_state = {
  .run = root_state_run,
};

/* Unclosable schema */

data(UnclosableSchema) {
  rich_Schema   base;
  rich_Schema*  wrap;
};
static rich_Schema_Impl unclosable_impl;

rich_Schema* rich_schema_unclosable(rich_Schema* wrap) {
  UnclosableSchema* self = malloc(sizeof(UnclosableSchema));
  self->base._impl = &unclosable_impl;
  self->wrap = wrap;
  return &self->base;
}
void rich_schema_close(rich_Schema* _self) {
  UnclosableSchema* self = (UnclosableSchema*)_self;
  call(self->wrap, close);
  free(self);
}
static size_t unclosable_data_size(void* _self) {
  UnclosableSchema* self = _self;
  return call(self->wrap, data_size);
}
static void unclosable_dump_value(void* _self, void* _value, rich_Sink* to) {
  UnclosableSchema* self = _self;
  call(self->wrap, dump_value, _value, to);
}
static void unclosable_reset_value(void* _self, void* value) {
  UnclosableSchema* self = _self;
  call(self->wrap, reset_value, value);
}
static void unclosable_close_value(void* _self, void* value) {
  UnclosableSchema* self = _self;
  call(self->wrap, close_value, value);
}
static void unclosable_push_state(void* _self, Coroutine* co, void* value) {
  UnclosableSchema* self = _self;
  call(self->wrap, push_state, co, value);
}
static rich_Schema_Impl unclosable_impl = {
  .data_size = unclosable_data_size,
  .dump_value = unclosable_dump_value,
  .close_value = unclosable_close_value,
  .reset_value = unclosable_reset_value,
  .close_value = unclosable_close_value,
  .push_state = unclosable_push_state,
  .close = null_close,
};

/* Bool */

static co_State bool_state;
static size_t bool_data_size(void* _self) {
  return sizeof(bool);
}
static void bool_dump_value(void* _self, void* _value, rich_Sink* to) {
  call(to, sink, RICH_BOOL, _value);
}
static void bool_reset_value(void* _self, void* _value) {
  *(bool*)_value = false;
}
static void bool_push_state(void* _self, Coroutine* co, void* value) {
  *(void**)coroutine_push(co, &bool_state, sizeof(void*)) = value;
}

static void bool_sink_run(void* udata, Coroutine* co, void* _arg) {
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_BOOL) RAISE(MALFORMED);
  bool* value = *(void**)udata;
  *value = *(bool*)arg->data;
  coroutine_pop(co);
}
static co_State bool_state = {
  .run = bool_sink_run,
};
static rich_Schema_Impl bool_impl = {
  .data_size = bool_data_size,
  .dump_value = bool_dump_value,
  .reset_value = bool_reset_value,
  .push_state = bool_push_state,
  .close = null_close,
};
rich_Schema rich_schema_bool[1] = {{
  ._impl = &bool_impl,
}};

/* Int64 */

static co_State int64_state;
static size_t int64_data_size(void* _self) {
  return sizeof(int64_t);
}
static void int64_dump_value(void* _self, void* _value, rich_Sink* to) {
  call(to, sink, RICH_INT, _value);
}
static void int64_reset_value(void* _self, void* _value) {
  *(int64_t*)_value = 0;
}
static void int64_push_state(void* _self, Coroutine* co, void* value) {
  *(void**)coroutine_push(co, &int64_state, sizeof(void*)) = value;
}

static void int64_sink_run(void* udata, Coroutine* co, void* _arg) {
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_INT) RAISE(MALFORMED);
  int64_t* value = *(void**)udata;
  *value = *(int64_t*)arg->data;
  coroutine_pop(co);
}
static co_State int64_state = {
  .run = int64_sink_run,
};
static rich_Schema_Impl int64_impl = {
  .data_size = int64_data_size,
  .dump_value = int64_dump_value,
  .reset_value = int64_reset_value,
  .push_state = int64_push_state,
  .close = null_close,
};
rich_Schema rich_schema_int64[1] = {{
  ._impl = &int64_impl,
}};

/* Double */

static co_State double_state;
static size_t double_data_size(void* _self) {
  return sizeof(double);
}
static void double_dump_value(void* _self, void* _value, rich_Sink* to) {
  call(to, sink, RICH_FLOAT, _value);
}
static void double_reset_value(void* _self, void* _value) {
  *(double*)_value = 0;
}
static void double_push_state(void* _self, Coroutine* co, void* value) {
  *(void**)coroutine_push(co, &double_state, sizeof(void*)) = value;
}

static void double_sink_run(void* udata, Coroutine* co, void* _arg) {
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_FLOAT) RAISE(MALFORMED);
  double* value = *(void**)udata;
  *value = *(double*)arg->data;
  coroutine_pop(co);
}
static co_State double_state = {
  .run = double_sink_run,
};
static rich_Schema_Impl double_impl = {
  .data_size = double_data_size,
  .dump_value = double_dump_value,
  .reset_value = double_reset_value,
  .push_state = double_push_state,
  .close = null_close,
};
rich_Schema rich_schema_double[1] = {{
  ._impl = &double_impl,
}};

/* Bytes */

static co_State bytes_state;
static size_t bytes_data_size(void* _self) {
  return sizeof(Bytes);
}
static void bytes_dump_value(void* _self, void* _value, rich_Sink* to) {
  call(to, sink, RICH_STRING, _value);
}
static void bytes_reset_value(void* _self, void* _value) {
  Bytes* value = _value;
  value->size = 0;
}
static void bytes_close_value(void* _self, void* _value) {
  Bytes* value = _value;
  if (value->ptr) {
    free(value->ptr);
    value->ptr = NULL;
  }
}
static void bytes_push_state(void* _self, Coroutine* co, void* value) {
  *(void**)coroutine_push(co, &bytes_state, sizeof(void*)) = value;
}

static void bytes_sink_run(void* udata, Coroutine* co, void* _arg) {
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_STRING) RAISE(MALFORMED);
  Bytes* value = *(void**)udata;
  Bytes* src = arg->data;
  if (!value->ptr) {
    value->cap = src->size;
    value->ptr = malloc(value->cap);
  } else if (src->size > value->cap) {
    value->cap = src->size;
    value->ptr = realloc(value->ptr, value->cap);
  }
  value->size = src->size;
  memcpy(value->ptr, src->ptr, value->size);
}
static co_State bytes_state = {
  .run = bytes_sink_run,
};
static rich_Schema_Impl bytes_impl = {
  .data_size = bytes_data_size,
  .dump_value = bytes_dump_value,
  .reset_value = bytes_reset_value,
  .close_value = bytes_close_value,
  .push_state = bytes_push_state,
  .close = null_close,
};
rich_Schema rich_schema_bytes[1] = {{
  ._impl = &bytes_impl,
}};

/* Pointer */

data(PointerSchema) {
  rich_Schema   base;
  rich_Schema*  of;
};
static rich_Schema_Impl pointer_impl;

rich_Schema* rich_schema_pointer(rich_Schema* of) {
  PointerSchema* self = malloc(sizeof(PointerSchema));
  self->base._impl = &pointer_impl;
  self->of = of;
  return &self->base;
}
static size_t pointer_data_size(void* _self) {
  return sizeof(void**);
}
static void pointer_dump_vaule(void* _self, void* value, rich_Sink* to) {
  PointerSchema* self = _self;
  void** ptr = value;
  if (*ptr) {
    call(self->of, dump_value, *ptr, to);
  } else {
    call(to, sink, RICH_NIL, NULL);
  }
}
static void pointer_reset_value(void* _self, void* value) {
  PointerSchema* self = _self;
  void** ptr = value;
  if (*ptr == NULL) {
    size_t sz = call(self->of, data_size);
    *ptr = calloc(sz, 1);
  }
  call(self->of, reset_value, *ptr);
}
static void pointer_close_value(void* _self, void* value) {
  PointerSchema* self = _self;
  void** ptr = value;
  if (*ptr) {
    call(self->of, close_value, *ptr);
    *ptr = NULL;
  }
}
static void pointer_push_state(void* _self, Coroutine* co, void* value) {
  PointerSchema* self = _self;
  void** ptr = value;
  assert(*ptr);
  call(self->of, push_state, co, *ptr);
}
static void pointer_close(void* _self) {
  PointerSchema* self = _self;
  call(self->of, close);
  free(self);
}
static rich_Schema_Impl pointer_impl = {
  .data_size = pointer_data_size,
  .dump_value = pointer_dump_vaule,
  .reset_value = pointer_reset_value,
  .close_value = pointer_close_value,
  .push_state = pointer_push_state,
  .close = pointer_close,
};

/* Optional */

data(OptionalSchema) {
  rich_Schema   base;
  rich_Schema*  wrap;
};
static rich_Schema_Impl optional_impl;

data(OptionalData) {
  rich_Schema*  wrap;
  void*         value;
};
static co_State optional_state;

rich_Schema* rich_schema_optional(rich_Schema* wrap) {
  OptionalSchema* self = malloc(sizeof(OptionalSchema));
  self->base._impl = &optional_impl;
  self->wrap = wrap;
  return &self->base;
}
static size_t optional_data_size(void* _self) {
  OptionalSchema* self = _self;
  return call(self->wrap, data_size);
}
static void optional_dump_value(void* _self, void* value, rich_Sink* to) {
  OptionalSchema* self = _self;
  call(self->wrap, dump_value, value, to);
}
static void optional_reset_value(void* _self, void* value) {
  OptionalSchema* self = _self;
  call(self->wrap, reset_value, value);
}
static void optional_close_value(void* _self, void* value) {
  OptionalSchema* self = _self;
  call(self->wrap, close_value, value);
}
static void optional_push_state(void* _self, Coroutine* co, void* value) {
  OptionalSchema* self = _self;
  OptionalData* data = coroutine_push(co, &optional_state, sizeof(OptionalData));
  data->wrap = self->wrap;
  data->value = value;
}
static void optional_close(void* _self) {
  OptionalSchema* self = _self;
  call(self->wrap, close);
  free(self);
}
static rich_Schema_Impl optional_impl = {
  .data_size = optional_data_size,
  .dump_value = optional_dump_value,
  .reset_value = optional_reset_value,
  .close_value = optional_close_value,
  .push_state = optional_push_state,
  .close = optional_close,
};

static void optional_state_run(void* udata, Coroutine* co, void* _arg) {
  OptionalData* data = udata;
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_NIL) {
    call(data->wrap, push_state, co, data->value);
    coroutine_run(co, arg);
  }
}
static co_State optional_state = {
  .run = optional_state_run,
};

/* Vector */

data(VectorSchema) {
  rich_Schema   base;
  rich_Schema*  of;
};
static rich_Schema_Impl vector_impl;

data(VectorData) {
  rich_Schema*  of;
  Vector*       v;
};
static co_State vector_state;

rich_Schema* rich_schema_vector(rich_Schema* of) {
  VectorSchema* self = malloc(sizeof(VectorSchema));
  self->base._impl = &vector_impl;
  self->of = of;
  return &self->base;
}
static size_t vector_data_size(void* _self) {
  return sizeof(Vector);
}
static void vector_dump_value(void* _self, void* _value, rich_Sink* to) {
  VectorSchema* self = _self;
  Vector* v = _value;
  if (!v->_data) {
    call(to, sink, RICH_NIL, NULL);
    return;
  }
  for (unsigned i = 0; i < v->size; i++) {
    call(self->of, dump_value, vector_get(v, i), to);
  }
  call(to, sink, RICH_ENDARRAY, NULL);
}
static void vector_reset_value(void* _self, void* _value) {
  VectorSchema* self = _self;
  Vector* v = _value;
  if (v->_data) {
    for (unsigned i = 0; i < v->size; i++) {
      call(self->of, close_value, vector_get(v, i));
    }
    vector_clear(v);
  } else {
    vector_init(v, call(self->of, data_size), 4);
  }
}
static void vector_close_value(void* _self, void* _value) {
  VectorSchema* self = _self;
  Vector* v = _value;
  if (v->_data) {
    for (unsigned i = 0; i < v->size; i++) {
      call(self->of, close_value, vector_get(v, i));
    }
    vector_close(v);
  }
}
static void vector_push_state(void* _self, Coroutine* co, void* _value) {
  VectorSchema* self = _self;
  VectorData* arg = coroutine_push(co, &vector_state, sizeof(VectorData));
  arg->of = self->of;
  arg->v = _value;
}
static void vector_schema_close(void* _self) {
  VectorSchema* self = _self;
  call(self->of, close);
  free(self);
}
static rich_Schema_Impl vector_impl = {
  .data_size = vector_data_size,
  .dump_value = vector_dump_value,
  .reset_value = vector_reset_value,
  .close_value = vector_close_value,
  .push_state = vector_push_state,
  .close = vector_schema_close,
};

static void vector_state_run(void* udata, Coroutine* co, void* _arg) {
  VectorData* data = udata;
  rich_SchemaArg* arg = _arg;
  if (data->v->size == 0) {
    if (arg->atom != RICH_ARRAY) RAISE(MALFORMED);
  }
  if (arg->atom == RICH_ENDARRAY) {
    coroutine_pop(co);
  } else {
    // Delegate to sub schema
    void* value = vector_push(data->v);
    call(data->of, push_state, co, value);
    coroutine_run(co, arg);
  }
}
static co_State vector_state = {
  .run = vector_state_run,
};

/* Hashtable */

data(HashtableSchema) {
  rich_Schema   base;
  rich_Schema*  of;
};
static rich_Schema_Impl hashtable_impl;

data(HashtableData) {
  rich_Schema*  of;
  Hashtable*    ht;
  Bytes         key[1];
};
static co_State hashtable_state;

rich_Schema* rich_schema_hashtable(rich_Schema* of) {
  HashtableSchema* self = malloc(sizeof(HashtableSchema));
  self->base._impl = &hashtable_impl;
  self->of = of;
  return &self->base;
}
static size_t hashtable_data_size(void* _self) {
  return sizeof(Hashtable);
}
static void hashtable_dump_value(void* _self, void* _value, rich_Sink* to) {
  HashtableSchema* self = _self;
  Hashtable* ht = _value;
  if (!ht->_buckets) {
    call(to, sink, RICH_NIL, NULL);
    return;
  }
  call(to, sink, RICH_MAP, NULL);
  int process_entry(void* _key, void* _data) {
    Bytes* key = _key;
    call(to, sink, RICH_KEY, key);
    call(self->of, dump_value, _data, to);
    return HT_CONTINUE;
  }
  hashtable_iter(ht, process_entry);
  call(to, sink, RICH_ENDMAP, NULL);
}
static void clear_hashtable(HashtableSchema* self, Hashtable* ht) {
  int free_entry(void* _key, void* _value) {
    Bytes* key = _key;
    free(key->ptr);
    call(self->of, close_value, _value);
    return HT_CONTINUE | HT_REMOVE;
  }
  hashtable_iter(ht, free_entry);
}
static void hashtable_reset_value(void* _self, void* _value) {
  HashtableSchema* self = _self;
  Hashtable* ht = _value;
  if (ht->_buckets) {
    clear_hashtable(self, ht);
  } else {
    hashtable_init(ht, hasher_bytes, equaler_bytes, sizeof(Bytes), call(self->of, data_size));
  }
}
static void hashtable_close_value(void* _self, void* _value) {
  HashtableSchema* self = _self;
  Hashtable* ht = _value;
  if (ht->_buckets) {
    clear_hashtable(self, ht);
    hashtable_close(ht);
  }
}
static void hashtable_push_state(void* _self, Coroutine* co, void* _value) {
  HashtableSchema* self = _self;
  Hashtable* ht = _value;
  HashtableData* data = coroutine_push(co, &hashtable_state, sizeof(HashtableData));
  data->of = self->of;
  data->ht = ht;
  bytes_init(data->key, 16);
}
static void hashtable_schema_close(void* _self) {
  HashtableSchema* self = _self;
  call(self->of, close);
  free(self);
}
static rich_Schema_Impl hashtable_impl = {
  .data_size = hashtable_data_size,
  .dump_value = hashtable_dump_value,
  .reset_value = hashtable_reset_value,
  .close_value = hashtable_close_value,
  .push_state = hashtable_push_state,
  .close = hashtable_schema_close,
};

static void hashtable_state_run(void* udata, Coroutine* co, void* _arg) {
  HashtableData* data = udata;
  rich_SchemaArg* arg = _arg;
  if (data->ht->size == 0) {
    if (arg->atom != RICH_MAP) RAISE(MALFORMED);
  }
  if (arg->atom == RICH_ENDMAP) {
    coroutine_pop(co);
  } else if (arg->atom == RICH_KEY) {
    bytes_copy(data->key, arg->data);
    // Delegate value to sub schema
    Bytes copy = {.ptr = NULL};
    bytes_copy(&copy, data->key);
    void* value = hashtable_insert(data->ht, &copy);
    call(data->of, push_state, co, value);
  } else {
    RAISE(MALFORMED);
  }
}
static void hashtable_state_close(void* udata) {
  HashtableData* data = udata;
  bytes_close(data->key);
}
static co_State hashtable_state = {
  .run = hashtable_state_run,
  .close = hashtable_state_close,
};

/* Structs */

data(StructSchema) {
  rich_Schema   base;
  size_t        data_size;
  Vector        fields[1];
};
static rich_Schema_Impl struct_impl;

data(Field) {
  Bytes         name[1];
  size_t        offset;
  rich_Schema*  schema;
};

data(StructData) {
  Vector*       fields;
  char*         data;
  bool          first_field;
  Bitset        read[];
};
static co_State struct_state;

static Field* find_field(Vector* fields, Bytes* name) {
  for (unsigned i = 0; i < fields->size; i++) {
    Field* f = vector_get(fields, i);
    if (bytes_compare(f->name, name) == 0) return f;
  }
  return NULL;
}

rich_Schema* rich_schema_struct(size_t data_size) {
  StructSchema* self = malloc(sizeof(StructSchema));
  self->base._impl = &struct_impl;
  self->data_size = data_size;
  vector_init(self->fields, sizeof(Field), 4);
  return &self->base;
}
void rich_add_field(rich_Schema* _self, Bytes name, size_t offset, rich_Schema* schema) {
  StructSchema* self = (StructSchema*)_self;
  assert(find_field(self->fields, &name) == NULL);
  Field* field = vector_push(self->fields);
  *field->name = name;
  field->offset = offset;
  field->schema = schema;
}
void rich_add_cfield(rich_Schema* self, const char* name, size_t offset, rich_Schema* schema) {
  Bytes bytes_name = {
    .ptr = (void*)name,
    .size = strlen(name),
  };
  rich_add_field(self, bytes_name, offset, schema);
}
static size_t struct_data_size(void* _self) {
  StructSchema* self = _self;
  return self->data_size;
}
static void struct_dump_value(void* _self, void* value, rich_Sink* to) {
  StructSchema* self = _self;
  char* data = value;
  call(to, sink, RICH_MAP, NULL);
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* field = vector_get(self->fields, i);
    call(to, sink, RICH_KEY, field->name);
    call(field->schema, dump_value, data + field->offset, to);
  }
  call(to, sink, RICH_ENDMAP, NULL);
}
static void struct_reset_value(void* _self, void* value) {
  StructSchema* self = _self;
  char* data = value;
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* field = vector_get(self->fields, i);
    call(field->schema, reset_value, data + field->offset);
  }
}
static void struct_close_value(void* _self, void* value) {
  StructSchema* self = _self;
  char* data = value;
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* field = vector_get(self->fields, i);
    call(field->schema, close_value, data + field->offset);
  }
}
static void struct_push_state(void* _self, Coroutine* co, void* value) {
  StructSchema* self = _self;
  size_t sz = sizeof(StructData) + bitset_size(self->fields->size);
  StructData* data = coroutine_push(co, &struct_state, sz);
  data->fields = self->fields;
  data->data = value;
  data->first_field = true;
  bitset_init(data->read, self->fields->size);
}
static void struct_close(void* _self) {
  StructSchema* self = _self;
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* field = vector_get(self->fields, i);
    call(field->schema, close);
  }
  vector_close(self->fields);
  free(self);
}
static rich_Schema_Impl struct_impl = {
  .data_size = struct_data_size,
  .dump_value = struct_dump_value,
  .reset_value = struct_reset_value,
  .close_value = struct_close_value,
  .push_state = struct_push_state,
  .close = struct_close,
};

static void struct_state_run(void* udata, Coroutine* co, void* _arg) {
  StructData* data = udata;
  rich_SchemaArg* arg = _arg;

  if (data->first_field) {
    data->first_field = false;
    if (arg->atom != RICH_MAP) RAISE(MALFORMED);
    return;
  }

  if (arg->atom == RICH_ENDMAP) {
    // Send NILs to all un-read fields
    for (unsigned i = 0; i < data->fields->size; i++) {
      if (bitset_get(data->read, i)) continue;
      Field* field = vector_get(data->fields, i);
      call(field->schema, push_state, co, data->data + field->offset);
      rich_SchemaArg fakenil = {
        .atom = RICH_NIL,
      };
      coroutine_run(co, &fakenil);
    }
    coroutine_pop(co);
    return;
  }

  if (arg->atom != RICH_KEY) RAISE(MALFORMED);
  Field* field = find_field(data->fields, arg->data);
  if (!field) RAISE(MALFORMED);
  call(field->schema, push_state, co, data->data + field->offset);
}
static co_State struct_state = {
  .run = struct_state_run,
};