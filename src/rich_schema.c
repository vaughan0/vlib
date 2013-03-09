
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rich_schema.h>
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
  Coroutine     co[1];
  rich_Schema*  schema;
  void*         to;
};
static rich_Sink_Impl bound_sink_impl;

static co_State_Impl sink_root_state;

rich_Sink* rich_bind_sink(rich_Schema* schema, void* to) {
  BoundSink* self = malloc(sizeof(BoundSink));
  self->base._impl = &bound_sink_impl;
  coroutine_init(self->co);
  self->schema = schema;
  self->to = to;
  coroutine_push(self->co, &sink_root_state, self);
  return &self->base;
}
static void bound_sink_sink(void* _self, rich_Atom atom, void* atom_data) {
  BoundSink* self = _self;
  call(self->schema, reset_value, self->to);
  rich_SchemaArg arg = {
    .value = self->to,
    .atom = atom,
    .data = atom_data,
  };
  coroutine_run(self->co, &arg);
}
static void bound_sink_close(void* _self) {
  BoundSink* self = _self;
  coroutine_close(self->co);
  if (self->schema->_impl->close_value) call(self->schema, close_value, self->to);
  if (self->schema->_impl->close) call(self->schema, close);
  free(self);
}
static rich_Sink_Impl bound_sink_impl = {
  .sink = bound_sink_sink,
  .close = bound_sink_close,
};

data(RootState) {
  co_State    base;
  BoundSink*  sink;
};
static void sink_root_init(void* _self, void* _arg) {
  RootState* self = _self;
  self->sink = _arg;
}
static void sink_root_run(void* _self, Coroutine* co, void* _arg) {
  RootState* self = _self;
  // Delegate to schema
  call(self->sink->schema, push_state, co);
  coroutine_run(co, _arg);
}
static co_State_Impl sink_root_state = {
  .size = sizeof(RootState),
  .init = sink_root_init,
  .run = sink_root_run,
};

/* Bool */

static co_State_Impl bool_state;
static void bool_dump_value(void* _self, void* _value, rich_Sink* to) {
  call(to, sink, RICH_BOOL, _value);
}
static void bool_reset_value(void* _self, void* _value) {
  *(bool*)_value = false;
}
static void bool_push_state(void* _self, Coroutine* co) {
  coroutine_push(co, &bool_state, NULL);
}

static void bool_sink_run(void* _self, Coroutine* co, void* _arg) {
  rich_SchemaArg* arg = _arg;
  if (arg->atom != RICH_BOOL) RAISE(MALFORMED);
  *(bool*)arg->value = *(bool*)arg->data;
  coroutine_pop(co);
}
static co_State_Impl bool_state = {
  .size = sizeof(co_State),
  .run = bool_sink_run,
};
static rich_Schema_Impl bool_impl = {
  .dump_value = bool_dump_value,
  .reset_value = bool_reset_value,
  .push_state = bool_push_state,
  .close = null_close,
};
rich_Schema rich_schema_bool[1] = {{
  ._impl = &bool_impl,
}};
