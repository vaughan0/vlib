
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rpc.h>
#include <vlib/io.h>

/* RPCService */

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

/* RPC <=> BinaryRPC mapping */

data(ClientRPC) {
  RPC           base;
  BinaryRPC*    backend;
  rich_Codec*   codec;
  Output*       argout;
  Output*       resultout;
  Input*        memin;
  rich_Sink*    arg_sink;
  rich_Source*  result_source;
};
static RPC_Impl client_impl;

RPC* rpc_from_binary(BinaryRPC* backend, rich_Codec* codec) {
  ClientRPC* self = malloc(sizeof(ClientRPC));
  self->base._impl = &client_impl;
  self->backend = backend;
  self->codec = codec;
  self->argout = string_output_new(512);
  self->resultout = string_output_new(512);
  self->memin = memory_input_new(NULL, 0);
  self->arg_sink = call(codec, new_sink, self->argout);
  self->result_source = call(codec, new_source, self->memin);
  return &self->base;
}

static void client_call(void* _self, const char* method, rich_Source* args, rich_Sink* result) {
  ClientRPC* self = _self;

  Bytes binary_method = {
    .ptr = (void*)method,
    .size = strlen(method)+1, // include terminating null byte
  };

  // Encode arguments to buffer
  string_output_reset(self->argout);
  call(args, read_value, self->arg_sink);
  Bytes binary_args;
  binary_args.ptr = (void*)string_output_data(self->argout, &binary_args.size);

  // Call backend and write result data to a buffer
  string_output_reset(self->resultout);
  call(self->backend, call, binary_method, binary_args, self->resultout);
  size_t result_size;
  const char* result_data = string_output_data(self->resultout, &result_size);

  // Decode result data
  memory_input_reset(self->memin, result_data, result_size);
  call(self->result_source, read_value, result);
}
static void client_close(void* _self) {
  ClientRPC* self = _self;
  call(self->arg_sink, close);
  call(self->result_source, close);
  call(self->resultout, close);
  call(self->codec, close);
  call(self->backend, close);
  free(self);
}
static RPC_Impl client_impl = {
  .call = client_call,
  .close = client_close,
};

data(ServerRPC) {
  BinaryRPC     base;
  RPC*          backend;
  rich_Codec*   codec;
  Input*        arg_in;
  rich_Source*  arg_source;
  Output*       result_out;
  rich_Sink*    result_sink;
};
static BinaryRPC_Impl server_impl;

BinaryRPC* rpc_to_binary(RPC* backend, rich_Codec* codec) {
  ServerRPC* self = malloc(sizeof(ServerRPC));
  self->base._impl = &server_impl;
  self->backend = backend;
  self->codec = codec;
  self->arg_in = memory_input_new(NULL, 0);
  self->arg_source = call(codec, new_source, self->arg_in);
  self->result_out = buf_output_new(NULL, 4096);
  self->result_sink = call(codec, new_sink, self->result_out);
  return &self->base;
}

static void server_call(void* _self, Bytes method, Bytes args, Output* result) {
  ServerRPC* self = _self;

  // Check method string
  const char* methodstr = method.ptr;
  if (method.size < 1) RAISE(MALFORMED);
  if (methodstr[method.size-1] != 0) RAISE(MALFORMED);

  memory_input_reset(self->arg_in, args.ptr, args.size);
  buf_output_reset(self->result_out, result);
  call(self->backend, call, methodstr, self->arg_source, self->result_sink);
  io_flush(self->result_out);
}
static void server_close(void* _self) {
  ServerRPC* self = _self;
  call(self->result_sink, close);
  call(self->arg_source, close);
  call(self->codec, close);
  call(self->backend, close);
  free(self);
}
static BinaryRPC_Impl server_impl = {
  .call = server_call,
  .close = server_close,
};

/* RPC over ZeroMQ */

#ifdef VLIB_ENABLE_ZMQ

data(RPCZMQ) {
  BinaryRPC     base;
  void*         socket;
};
static BinaryRPC_Impl zmq_impl;

BinaryRPC* rpc_zmq_new(void* req_socket) {
  RPCZMQ* self = malloc(sizeof(RPCZMQ));
  self->base._impl = &zmq_impl;
  self->socket = req_socket;
  return &self->base;
}

static void zmqrpc_call(void* _self, Bytes method, Bytes args, Output* result) {
  RPCZMQ* self = _self;
  zmq_msg_t msg[1];
  int r;

  // Send method string as a message part
  zmq_msg_init_size(msg, method.size);
  memcpy(zmq_msg_data(msg), method.ptr, method.size);
  r = zmq_send(self->socket, msg, ZMQ_SNDMORE);
  if (r == -1) verr_raise_system();

  // Send argument data as a message part
  zmq_msg_init_size(msg, args.size);
  memcpy(zmq_msg_data(msg), args.ptr, args.size);
  r = zmq_send(self->socket, msg, 0);
  if (r == -1) verr_raise_system();

  // Wait for result data
  zmq_msg_init(msg);
  r = zmq_recv(self->socket, msg, 0);
  if (r == -1) verr_raise_system();
  io_write(result, zmq_msg_data(msg), zmq_msg_size(msg));
  zmq_msg_close(msg);
}
static void zmqrpc_close(void* _self) {
  RPCZMQ* self = _self;
  zmq_close(self->socket);
  free(self);
}
static BinaryRPC_Impl zmq_impl = {
  .call = zmqrpc_call,
  .close = zmqrpc_close,
};

#endif
