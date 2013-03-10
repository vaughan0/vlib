
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

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

  // Wait for result data
  zmq_msg_t msg[1];
  zmq_msg_init(msg);
  r = zmq_msg_recv(msg, self->socket, 0);
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
      r = zmq_send(socket, resp_data, resp_size, 0);
      if (r == -1) verr_raise_system();

    }
  } FINALLY {
    call(buf, close);
  } ETRY
}

#endif
