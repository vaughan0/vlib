
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vlib/rpc.h>
#include <vlib/io.h>

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

/* RPC_Client */

data(ClientMethod) {
  const char*   name;
  rich_Schema*  arg_schema;
  rich_Source*  arg_source;
  rich_Schema*  result_schema;
  rich_Sink*    result_sink;
};

void rpc_init(RPC_Client* self, RPC* backend) {
  self->backend = backend;
  vector_init(self->methods, sizeof(ClientMethod), 4);
}
void rpc_close(RPC_Client* self) {
  for (unsigned i = 0; i < self->methods->size; i++) {
    ClientMethod* method = vector_get(self->methods, i);
    call(method->arg_source, close);
    call(method->arg_schema, close);
    call(method->result_sink, close);
    call(method->result_schema, close);
  }
  vector_close(self->methods);
}
int rpc_register(RPC_Client* self, const char* method_name, rich_Schema* arg_schema, rich_Schema* result_schema) {
  ClientMethod* method = vector_push(self->methods);
  method->name = method_name;
  method->arg_schema = arg_schema;
  method->arg_source = rich_schema_source(arg_schema, NULL);
  method->result_schema = result_schema;
  method->result_sink = rich_bind(result_schema, NULL);
  return self->methods->size - 1;
}

void rpc_call(RPC_Client* self, int method_index, void* arg, void* result) {
  ClientMethod* method = vector_get(self->methods, method_index);
  call(method->arg_schema, close_data, arg);
  rich_schema_reset(method->arg_source, arg);
  call(method->result_schema, close_data, result);
  rich_rebind(method->result_sink, result);
  call(self->backend, call, method->name, method->arg_source, method->result_sink);
}

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

static bool hasmore(void* socket) {
  int64_t rcvmore;
  size_t optlen = sizeof(rcvmore);
  int r = zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore, &optlen);
  assert(r != -1);
  return rcvmore != 0;
}
static void discard_parts(void* socket) {
  while (hasmore(socket)) {
    zmq_msg_t msg[1];
    zmq_msg_init_size(msg, 8);
    int r = zmq_recv(socket, msg, 0);
    if (r == -1) verr_raise_system();
    zmq_msg_close(msg);
  }
}
void rpc_zmq_serve(void* socket, BinaryRPC* rpc) {
  int r;
  Output* buf = string_output_new(512);
  TRY {
    for (;; discard_parts(socket)) {

      // Receive method string
      zmq_msg_t method[1];
      zmq_msg_init(method);
      r = zmq_recv(socket, method, 0);
      if (r == -1) verr_raise_system();
      if (!hasmore(socket)) {
        zmq_msg_close(method);
        continue;
      }

      // Receive argument data
      zmq_msg_t args[1];
      zmq_msg_init(args);
      r = zmq_recv(socket, args, 0);
      if (r == -1) verr_raise_system();
      if (!hasmore(socket)) {
        zmq_msg_close(method);
        zmq_msg_close(args);
        continue;
      }

      // Process request
      Bytes method_bytes = {
        .ptr = zmq_msg_data(method),
        .size = zmq_msg_size(method),
      };
      Bytes arg_bytes = {
        .ptr = zmq_msg_data(args),
        .size = zmq_msg_size(args),
      };
      TRY {
        string_output_reset(buf);
        call(rpc, call, method_bytes, arg_bytes, buf);
      } FINALLY {
        zmq_msg_close(method);
        zmq_msg_close(args);
      } ETRY

      // Return response
      size_t resp_size;
      const char* resp_data = string_output_data(buf, &resp_size);
      zmq_msg_t resp[1];
      zmq_msg_init_size(resp, resp_size);
      memcpy(zmq_msg_data(resp), resp_data, resp_size);
      r = zmq_send(socket, resp, 0);
      if (r == -1) verr_raise_system();

    }
  } FINALLY {
    call(buf, close);
  } ETRY
}

#endif
