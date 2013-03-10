
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <vlib/rpc.h>
#include <vlib/io.h>
#include <vlib/util.h>
#include <vlib/logging.h>

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
    call(method->result_sink, close);
  }
  vector_close(self->methods);
  call(self->backend, close);
}
int rpc_register(RPC_Client* self, const char* method, rich_Schema* arg_schema, rich_Schema* result_schema) {
  ClientMethod* m = vector_push(self->methods);
  m->name = method;
  m->arg_schema = arg_schema;
  m->arg_source = rich_bind_source(arg_schema, NULL);
  m->result_schema = result_schema;
  m->result_sink = rich_bind_sink(result_schema, NULL);
  return self->methods->size-1;
}

void rpc_call(RPC_Client* self, int method, void* arg, void* result) {
  ClientMethod* m = vector_get(self->methods, method);
  rich_rebind_source(m->arg_source, arg);
  rich_rebind_sink(m->result_sink, result);
  call(self->backend, call, m->name, m->arg_source, m->result_sink);
}

/* RPC_Service */

data(ServiceMethod) {
  RPCMethod     func;
  rich_Schema*  arg_schema;
  rich_Sink*    arg_sink;
  void*         arg_data;
  rich_Schema*  result_schema;
  rich_Source*  result_source;
  void*         result_data;
};

data(RPC_Service) {
  RPC       base;
  void      (*cleanup)(void* udata);
  void*     udata;
  Hashtable methods[1];
};
static RPC_Impl service_impl;

RPC* rpc_service_new(void* udata) {
  RPC_Service* self = malloc(sizeof(RPC_Service));
  self->base._impl = &service_impl;
  self->cleanup = NULL;
  self->udata = udata;
  hashtable_init(self->methods, hasher_fnv64str, equaler_str, sizeof(const char*), sizeof(ServiceMethod));
  return &self->base;
}
void rpc_service_cleanup(RPC* _self, void (*cleanup_handler)(void* udata)) {
  RPC_Service* self = (RPC_Service*)_self;
  self->cleanup = cleanup_handler;
}
void rpc_add(RPC* _self, const char* method, RPCMethod func, rich_Schema* arg_schema, rich_Schema* result_schema) {
  RPC_Service* self = (RPC_Service*)_self;
  assert(hashtable_get(self->methods, &method) == NULL);
  ServiceMethod* m = hashtable_insert(self->methods, &method);
  m->func = func;

  m->arg_schema = arg_schema;
  size_t argsz = call(arg_schema, data_size);
  m->arg_data = malloc(argsz);
  memset(m->arg_data, 0, argsz);
  m->arg_sink = rich_bind_sink(arg_schema, m->arg_data);

  m->result_schema = result_schema;
  size_t resultsz = call(result_schema, data_size);
  m->result_data = malloc(resultsz);
  memset(m->result_data, 0, resultsz);
  m->result_source = rich_bind_source(result_schema, m->result_data);
}

static void service_call(void* _self, const char* method, rich_Source* arg, rich_Sink* result) {
  RPC_Service* self = _self;
  ServiceMethod* m = hashtable_get(self->methods, &method);
  if (!m) verr_raisef(VERR_ARGUMENT, "unknown method '%s'", method);
  call(arg, read_value, m->arg_sink);
  call(m->result_schema, reset_value, m->result_data);
  m->func(self->udata, m->arg_data, m->result_data);
  call(m->result_source, read_value, result);
}
static void service_close(void* _self) {
  RPC_Service* self = _self;
  int free_method(void* _key, void* _data) {
    ServiceMethod* method = _data;
    call(method->arg_sink, close);
    free(method->arg_data);
    call(method->result_source, close);
    call(method->result_schema, close_value, method->result_data);
    free(method->result_data);
    return HT_CONTINUE;
  }
  hashtable_iter(self->methods, free_method);
  if (self->cleanup) self->cleanup(self->udata);
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
  int r;

  // Send method string as a message part
  r = zmq_send(self->socket, method.ptr, method.size, ZMQ_SNDMORE);
  if (r == -1) verr_raise_system();

  // Send argument data as a message part
  r = zmq_send(self->socket, args.ptr, args.size, 0);
  if (r == -1) verr_raise_system();

  // Wait for status code
  zmq_msg_t msg[1];
  zmq_msg_init(msg);
  r = zmq_msg_recv(msg, self->socket, 0);
  if (r == -1) verr_raise_system();
  if (zmq_msg_size(msg) != 1) verr_raisef(VERR_MALFORMED, "malformed RPC response");
  if (*(bool*)zmq_msg_data(msg) != true) {
    // Raise remote error
    char errstr[512];
    strncpy(errstr, zmq_msg_data(msg), MIN(sizeof(errstr), zmq_msg_size(msg)));
    zmq_msg_close(msg);
    verr_raisef(VERR_REMOTE, "%s", errstr);
  }

  // Return result data
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
  int rcvmore;
  size_t optlen = sizeof(rcvmore);
  int r = zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore, &optlen);
  assert(r != -1);
  return rcvmore != 0;
}
static void discard_parts(void* socket) {
  while (hasmore(socket)) {
    zmq_msg_t msg[1];
    zmq_msg_init_size(msg, 8);
    int r = zmq_msg_recv(msg, socket, 0);
    if (r == -1) verr_raise_system();
    zmq_msg_close(msg);
  }
}
void rpc_zmq_serve(void* socket, BinaryRPC* rpc) {
  int r;
  Output* buf = string_output_new(512);
  Logger* log = get_logger("vlib.rpc.zmq");
  log_debug(log, "entering rpc_zmq_serve()...");
  TRY {
    for (;; discard_parts(socket)) {

      // Receive method string
      zmq_msg_t method[1];
      zmq_msg_init(method);
      r = zmq_msg_recv(method, socket, 0);
      if (r == -1) verr_raise_system();
      if (!hasmore(socket)) {
        zmq_msg_close(method);
        RAISE(MALFORMED);
      }

      // Receive argument data
      zmq_msg_t args[1];
      zmq_msg_init(args);
      r = zmq_msg_recv(args, socket, 0);
      if (r == -1) verr_raise_system();
      if (hasmore(socket)) {
        zmq_msg_close(method);
        zmq_msg_close(args);
        RAISE(MALFORMED);
      }

      char cbuf[1024];
      memcpy(cbuf, zmq_msg_data(args), zmq_msg_size(args));
      cbuf[zmq_msg_size(args)] = 0;

      // Process request
      Bytes method_bytes = {
        .ptr = zmq_msg_data(method),
        .size = zmq_msg_size(method),
      };
      Bytes arg_bytes = {
        .ptr = zmq_msg_data(args),
        .size = zmq_msg_size(args),
      };
      bool ok = true;
      TRY {
        string_output_reset(buf);
        call(rpc, call, method_bytes, arg_bytes, buf);
      } CATCH(err) {
        log_warnf(log, "RPC method error: %s", verr_current_str());
        ok = false;
        string_output_reset(buf);
        io_writec(buf, verr_current_str());
      } ETRY
      zmq_msg_close(method);
      zmq_msg_close(args);

      // Send status message
      r = zmq_send(socket, &ok, 1, ZMQ_SNDMORE);
      if (r == -1) verr_raise_system();

      // Send result data
      size_t resp_size;
      const char* resp_data = string_output_data(buf, &resp_size);
      r = zmq_send(socket, resp_data, resp_size, 0);
      if (r == -1) verr_raise_system();

    }
  } FINALLY {
    call(buf, close);
  } ETRY
}

#endif
