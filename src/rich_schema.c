
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rich_schema.h>
#include <vlib/hashtable.h>
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
  if (arg->atom == RICH_ARRAY) {
    return;
  } else if (arg->atom == RICH_ENDARRAY) {
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
  if (arg->atom == RICH_MAP) {
    return;
  } else if (arg->atom == RICH_ENDMAP) {
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
static co_State hashtable_state = {
  .run = hashtable_state_run,
};