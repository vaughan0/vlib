#ifndef RPC_H_4EFFD6B83F9CFC
#define RPC_H_4EFFD6B83F9CFC

#include <vlib/rich.h>
#include <vlib/rich_bind.h>
#include <vlib/hashtable.h>

interface(RPC) {
  void  (*call)(void* self, const char* method, rich_Source* args, rich_Sink* result);
  void  (*close)(void* self);
};

RPC*    rpc_unclosable_new(RPC* wrap);
void    rpc_unclosable_close(RPC* self);

RPC*    rpc_mux_new();
void    rpc_mux_add(RPC* self, const char* prefix, RPC* delegate);

/* RPC Service, for server-side RPC code */

typedef void (*RPCMethod)(void* udata, void* args, void* result);

RPC*    rpc_service_new(void* udata);
void    rpc_service_add(RPC* self, const char* method, RPCMethod handler, rich_Schema* arg_schema, rich_Schema* result_schema);

/* Client-side RPC */

interface(BinaryRPC) {
  void  (*call)(void* self, Bytes method, Bytes args, Output* result);
  void  (*close)(void* self);
};

RPC*        rpc_from_binary(BinaryRPC* backend, rich_Codec* codec);
BinaryRPC*  rpc_to_binary(RPC* backend, rich_Codec* codec);

#ifdef VLIB_ENABLE_ZMQ

#include <zmq.h>

BinaryRPC*  rpc_zmq_new(void* req_socket);

#endif

#endif /* RPC_H_4EFFD6B83F9CFC */

