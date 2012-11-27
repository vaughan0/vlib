
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vlib/rich_bind.h>
#include <vlib/vector.h>

void rich_dump(rich_Schema* schema, void* from, rich_Sink* to) {
  call(schema, dump_value, from, to);
}

data(BoundSink) {
  rich_Sink     base;
  rich_Schema*  schema;
  rich_Reactor  reactor[1];
};

static rich_Sink_Impl bound_sink_impl;

rich_Sink* rich_bind(rich_Schema* schema, void* to) {
  BoundSink* sink = v_malloc(sizeof(BoundSink));
  sink->base._impl = &bound_sink_impl;
  sink->schema = schema;
  TRY {
    rich_reactor_init(sink->reactor, sizeof(rich_Frame));
    rich_schema_push(sink->reactor, schema, to);
  } CATCH(err) {
    rich_reactor_close(sink->reactor);
    free(sink);
    verr_raise(err);
  } ETRY
  return &sink->base;
}
void rich_rebind(rich_Sink* _self, void* to) {
  BoundSink* self = (BoundSink*)_self;
  rich_reactor_reset(self->reactor);
  rich_schema_push(self->reactor, self->schema, to);
}

static void bound_close(void* _self) {
  BoundSink* self = _self;
  rich_reactor_close(self->reactor);
  free(self);
}
static void bound_sink(void* _self, rich_Atom atom, void* atom_data) {
  BoundSink* self = _self;
  call(&self->reactor->base, sink, atom, atom_data);
}

static rich_Sink_Impl bound_sink_impl = {
  .sink = bound_sink,
  .close = bound_close,
};

/* bool */

static void bool_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  if (atom != RICH_BOOL) RAISE(MALFORMED);
  bool* to = ((rich_Frame*)r->data)->to;
  *to = *(bool*)data;
  rich_reactor_pop(r);
}
static void bool_dump(void* _self, void* from, rich_Sink* to) {
  call(to, sink, RICH_BOOL, from);
}
static size_t bool_size(void* _self) {
  return sizeof(bool);
}
static rich_Schema_Impl bool_impl = {
  .sink_impl.sink = bool_sink,
  .dump_value = bool_dump,
  .data_size = bool_size,
};
rich_Schema rich_schema_bool = {
  ._impl = &bool_impl,
};

/* int */

static void int_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  if (atom != RICH_INT) RAISE(MALFORMED);
  int* to = ((rich_Frame*)r->data)->to;
  *to = *(int*)data;
  rich_reactor_pop(r);
}
static void int_dump(void* _self, void* from, rich_Sink* to) {
  call(to, sink, RICH_INT, from);
}
static size_t int_size(void* _self) {
  return sizeof(int);
}
static rich_Schema_Impl int_impl = {
  .sink_impl.sink = int_sink,
  .dump_value = int_dump,
  .data_size = int_size,
};
rich_Schema rich_schema_int = {
  ._impl = &int_impl,
};

/* float */

static void float_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  if (atom != RICH_FLOAT) RAISE(MALFORMED);
  double* to = ((rich_Frame*)r->data)->to;
  *to = *(double*)data;
  rich_reactor_pop(r);
}
static void float_dump(void* _self, void* from, rich_Sink* to) {
  call(to, sink, RICH_FLOAT, from);
}
static size_t float_size(void* _self) {
  return sizeof(double);
}
static rich_Schema_Impl float_impl = {
  .sink_impl.sink = float_sink,
  .dump_value = float_dump,
  .data_size = float_size,
};
rich_Schema rich_schema_float = {
  ._impl = &float_impl,
};

/* string */

static void string_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  rich_String* to = ((rich_Frame*)r->data)->to;
  if (atom == RICH_STRING) {
    rich_String* from = data;
    to->sz = from->sz;
    to->data = v_malloc(from->sz);
    memcpy(to->data, from->data, from->sz);
  } else if (atom == RICH_NIL) {
    to->sz = 0;
    to->data = NULL;
  } else RAISE(MALFORMED);
  rich_reactor_pop(r);
}
static void string_dump(void* _self, void* from, rich_Sink* to) {
  rich_String* str = from;
  if (str->data)
    call(to, sink, RICH_STRING, str);
  else
    call(to, sink, RICH_NIL, NULL);
}
static size_t string_size(void* _self) {
  return sizeof(rich_String);
}
static rich_Schema_Impl string_impl = {
  .sink_impl.sink = string_sink,
  .dump_value = string_dump,
  .data_size = string_size,
};
rich_Schema rich_schema_string = {
  ._impl = &string_impl,
};

/* cstring */

static void cstring_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  char** to = ((rich_Frame*)r->data)->to;
  if (atom == RICH_STRING) {
    rich_String* from = data;
    *to = v_malloc(from->sz + 1);
    memcpy(*to, from->data, from->sz);
    to[from->sz] = 0;
  } else if (atom == RICH_NIL) {
    *to = NULL;
  } else RAISE(MALFORMED);
  rich_reactor_pop(r);
}
static void cstring_dump(void* _self, void* from, rich_Sink* to) {
  char* cstr = *(char**)from;
  if (cstr) {
    rich_String s = {
      .sz = strlen(cstr),
      .data = cstr,
    };
    call(to, sink, RICH_STRING, &s);
  } else {
    call(to, sink, RICH_NIL, NULL);
  }
}
static size_t cstring_size(void* _self) {
  return sizeof(char*);
}
static rich_Schema_Impl cstring_impl = {
  .sink_impl.sink = cstring_sink,
  .dump_value = cstring_dump,
  .data_size = cstring_size,
};
rich_Schema rich_schema_cstring = {
  ._impl = &cstring_impl,
};

/* Vector */

static rich_Schema_Impl vector_impl;

data(VectorSchema) {
  rich_Schema   base;
  rich_Schema*  of;
};

rich_Schema* rich_schema_vector(rich_Schema* of) {
  VectorSchema* self = v_malloc(sizeof(VectorSchema));
  self->base._impl = &vector_impl;
  self->of = of;
  return &self->base;
};

static void vector_init_frame(void* _self, void* data) {
  rich_Frame* f = data;
  f->udata = 0;
}
static void vector_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  VectorSchema* self = _self;
  rich_Frame* frame = r->data;
  Vector* v = frame->to;
  if (frame->udata == 0) {
    if (atom != RICH_ARRAY) RAISE(MALFORMED);
    vector_init(v, call(self->of, data_size), 7);
    frame->udata = (void*)1;
  } else if (atom == RICH_ENDARRAY) {
    rich_reactor_pop(r);
  } else {
    void* elem = vector_push(v);
    rich_schema_push(r, self->of, elem);
    call((rich_Reactor_Sink*)self->of, sink, r, atom, data);
  }
}
static void vector_dump(void* _self, void* from, rich_Sink* to) {
  VectorSchema* self = _self;
  Vector* v = from;
  call(to, sink, RICH_ARRAY, NULL);
  for (unsigned i = 0; i < v->size; i++) {
    call(self->of, dump_value, vector_get(v, i), to);
  }
  call(to, sink, RICH_ENDARRAY, NULL);
}
static size_t vector_size(void* _self) {
  return sizeof(Vector);
}
static void _vector_close(void* _self) {
  VectorSchema* self = _self;
  rich_schema_close(self->of);
  free(self);
}
static rich_Schema_Impl vector_impl = {
  .sink_impl.sink = vector_sink,
  .sink_impl.init_frame = vector_init_frame,
  .dump_value = vector_dump,
  .data_size = vector_size,
  .close = _vector_close,
};

/* Hashtable */

static rich_Schema_Impl hashtable_impl;

data(HashtableSchema) {
  rich_Schema   base;
  rich_Schema*  of;
};

rich_Schema* rich_schema_hashtable(rich_Schema* of) {

}

/* struct */

static rich_Schema_Impl struct_impl;

data(StructSchema) {
  rich_Schema base;
  size_t      data_size;
  bool        ignore_unknown;
  Vector      fields[1];
};

data(Field) {
  rich_String   name;
  rich_Schema*  schema;
  size_t        offset;
};

rich_Schema* rich_schema_struct(size_t sz) {
  StructSchema* self = v_malloc(sizeof(StructSchema));
  self->base._impl = &struct_impl;
  self->data_size = sz;
  self->ignore_unknown = false;
  TRY {
    vector_init(self->fields, sizeof(Field), 7);
  } CATCH(err) {
    vector_close(self->fields);
    free(self);
    verr_raise(err);
  } ETRY
  return &self->base;
}

void rich_struct_set_ignore_unknown(void* _self, bool ignore) {
  StructSchema* self = _self;
  self->ignore_unknown = ignore;
}

void rich_struct_register(void* _self, rich_String* name, size_t offset, rich_Schema* schema) {
  StructSchema* self = _self;
  Field* field = vector_push(self->fields);
  field->schema = schema;
  field->offset = offset;

  field->name.sz = name->sz;
  field->name.data = NULL;
  field->name.data = v_malloc(name->sz);
  memcpy(field->name.data, name->data, name->sz);
}
void rich_struct_cregister(void* _self, const char* name, size_t offset, rich_Schema* schema) {
  rich_String str = {
    .sz = strlen(name),
    .data = (char*)name,
  };
  rich_struct_register(_self, &str, offset, schema);
}

static void struct_init_frame(void* _self, void* data) {
  rich_Frame* frame = data;
  frame->udata = 0;
}
static void struct_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* data) {
  StructSchema* self = _self;
  rich_Frame* frame = r->data;
  void* to = frame->to;
  if (frame->udata == 0) {
    if (atom != RICH_MAP) RAISE(MALFORMED);
    memset(to, 0, self->data_size);
    frame->udata = (void*)1;
  } else if (atom == RICH_ENDMAP) {
    rich_reactor_pop(r);
  } else if (atom == RICH_KEY) {

    // Find the corresponding field
    rich_String* name = data;
    Field* field = NULL;
    for (unsigned i = 0; i < self->fields->size; i++) {
      Field* f = vector_get(self->fields, i);
      if (rich_string_equal(&f->name, name)) {
        field = f;
        break;
      }
    }
    if (!field) {
      if (self->ignore_unknown) {
        return;
      }
      RAISE(MALFORMED);
    }

    // Delegate for the next atom
    rich_schema_push(r, field->schema, (char*)to + field->offset);

  } else {
    RAISE(MALFORMED);
  }
}
static void struct_dump(void* _self, void* from, rich_Sink* to) {
  StructSchema* self = _self;
  call(to, sink, RICH_MAP, NULL);
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* field = vector_get(self->fields, i);
    call(to, sink, RICH_KEY, &field->name);
    call(field->schema, dump_value, (char*)from + field->offset, to);
  }
  call(to, sink, RICH_ENDMAP, NULL);
}
static size_t struct_size(void* _self) {
  StructSchema* self = _self;
  return self->data_size;
}
static void struct_close(void* _self) {
  StructSchema* self = _self;
  for (unsigned i = 0; i < self->fields->size; i++) {
    Field* f = vector_get(self->fields, i);
    rich_schema_close(f->schema);
    free(f->name.data);
  }
  vector_close(self->fields);
  free(self);
}

static rich_Schema_Impl struct_impl = {
  .sink_impl.sink = struct_sink,
  .sink_impl.init_frame = struct_init_frame,
  .dump_value = struct_dump,
  .data_size = struct_size,
  .close = struct_close,
};

/* pointer */

static rich_Schema_Impl pointer_impl;

data(PointerSchema) {
  rich_Schema   base;
  rich_Schema*  sub;
};

rich_Schema* rich_schema_pointer(rich_Schema* sub) {
  PointerSchema* self = v_malloc(sizeof(PointerSchema));
  self->base._impl = &pointer_impl;
  self->sub = sub;
  return &self->base;
}

static void pointer_sink(void* _self, rich_Reactor* r, rich_Atom atom, void* atom_data) {
  PointerSchema* self = _self;
  void** to = ((rich_Frame*)r->data)->to;
  if (atom == RICH_NIL) {
    *to = NULL;
    rich_reactor_pop(r);
  } else {
    size_t sz = call(self->sub, data_size);
    *to = v_malloc(sz);
    memset(*to, 0, sz);
    rich_reactor_pop(r);
    rich_schema_push(r, self->sub, *to);
    call((rich_Reactor_Sink*)self->sub, sink, r, atom, atom_data);
  }
}
static void pointer_dump(void* _self, void* from, rich_Sink* to) {
  PointerSchema* self = _self;
  from = *(void**)from;
  if (from)
    call(self->sub, dump_value, from, to);
  else
    call(to, sink, RICH_NIL, NULL);
}
static size_t pointer_size(void* _self) {
  return sizeof(void*);
}
static void pointer_close(void* _self) {
  PointerSchema* self = _self;
  rich_schema_close(self->sub);
  free(self);
}

static rich_Schema_Impl pointer_impl = {
  .sink_impl.sink = pointer_sink,
  .dump_value = pointer_dump,
  .data_size = pointer_size,
  .close = pointer_close,
};
