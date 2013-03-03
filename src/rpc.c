
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
}

static void service_call(void* _self, const char* method, rich_Source* args, rich_Sink* result) {

}
