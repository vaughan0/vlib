
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rpc.h>

data(Method) {
  RPCMethod     handler;
  rich_Schema*  arg_schema;
  rich_Schema*  result_schema;
  void*         args;
  void*         results;
  rich_Sink*    arg_sink;
};

data(RPCService) {
  RPC       base;
  void*     udata;
  Hashtable methods[1];
};
static RPC_Impl service_impl;

RPC* rpc_service_new(void* udata) {
  RPCService* self = malloc(sizeof(RPCService));
  self->base._impl = &service_impl;
  self->udata = udata;
  hashtable_init(self->methods, hasher_fnv64str, equaler_str, sizeof(char**), sizeof(RPCService));
  return &self->base;
}
void rpc_service_add(RPC* _self, const char* method, RPCMethod handler, rich_Schema* arg_schema, rich_Schema* result_schema) {
  RPCService* self = (RPCService*)_self;
  char* copy = strdup(method);
  Method* m = hashtable_insert(self->methods, &copy);
  m->handler = handler;
  m->arg_schema = arg_schema;
  m->result_schema = result_schema;
  m->args = calloc(1, call(arg_schema, data_size));
  m->results = calloc(1, call(result_schema, data_size));
  m->arg_sink = rich_bind(arg_schema, m->args);
}

static void service_call(void* _self, const char* method, rich_Source* arg_source, rich_Sink* result_sink) {
  RPCService* self = _self;
  Method* m = hashtable_get(self->methods, &method);
  if (!m) RAISE(UNAVAILABLE);

  call(m->arg_schema, close_data, m->args);
  call(m->result_schema, close_data, m->results);
  call(arg_source, read_value, m->arg_sink);

  m->handler(self->udata, m->args, m->results);

  rich_dump(m->result_schema, m->results, result_sink);
}
static void service_close(void* _self) {
  RPCService* self = _self;
  int free_method(void* _key, void* _data) {
    char* key = *(char**)_key;
    free(key);
    Method* m = _data;
    call(m->arg_schema, close_data, m->args);
    free(m->args);
    call(m->result_schema, close_data, m->results);
    free(m->results);
    call(m->arg_sink, close);
    call(m->arg_schema, close);
    call(m->result_schema, close);
    return HT_CONTINUE;
  }
  hashtable_iter(self->methods, free_method);
  hashtable_close(self->methods);
  free(self);
}
static RPC_Impl service_impl = {
  .call = service_call,
  .close = service_close,
};
