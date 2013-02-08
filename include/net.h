#ifndef NET_H_FA4D5B9E6863EB
#define NET_H_FA4D5B9E6863EB

#include <stdint.h>

#include <vlib/std.h>
#include <vlib/io.h>
#include <vlib/error.h>

// Networking error codes
enum {
  VERR_NET        = VERR_MAKE(VERR_PNET, 1),    // general networking error
  VERR_NET_INUSE  = VERR_MAKE(VERR_PNET, 2),    // address in use
  VERR_NET_LOOKUP = VERR_MAKE(VERR_PNET, 3),    // address lookup error
};

void net_register_errors() __attribute__((constructor));

data(NetConn_Impl) {
  void  (*close)(void* self);
};

data(NetConn) {
  NetConn_Impl* _impl;
  Input*  input;
  Output* output;
};

interface(NetListener) {
  NetConn*  (*accept)(void* self);
  void      (*close)(void* self);
};

NetListener*  net_listen_tcp(const char* addr, int port);

#endif /* NET_H_FA4D5B9E6863EB */

