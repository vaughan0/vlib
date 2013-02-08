
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "vlib/net.h"
#include "vlib/error.h"

/* Networking errors */

static const char* net_get_msg(error_t err) {
  switch (err) {
  case VERR_NET:        return "networking error";
  case VERR_NET_INUSE:  return "network address in use";
  case VERR_NET_LOOKUP: return "network address lookup error";
  }
  return NULL;
}

static ErrorProvider net_provider = {
  .name = "net",
  .get_msg = net_get_msg,
};

void net_register_errors() {
  verr_register(VERR_PNET, &net_provider);
}

/* TCP */

data(TCPListener) {
  NetListener_Impl* _impl;
  int socket;
};

static NetListener_Impl tcp_listener_impl;

data(TCPConn) {
  NetConn base;
  int     fd;
};

static NetConn_Impl tcp_conn_impl;

NetListener* net_listen_tcp(const char* addr, int port) {
  TCPListener* self = v_malloc(sizeof(TCPListener));

  // Lookup address
  struct addrinfo hints = {
    .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo* lookup;
  int r = getaddrinfo(addr, NULL, &hints, &lookup);
  if (r) {

  }

  int sock = socket(AF_INET6, SOCK_STREAM, 0);
  if (sock == -1) goto Error;

  struct sockaddr_in6 bind_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(port),
  };
  memcpy(bind_addr.sin6_addr.s6_addr, addr, sizeof(addr));
  r = bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
  if (r == -1) goto Error;

  r = listen(sock, 5);
  if (r == -1) goto Error;

  self->_impl = &tcp_listener_impl;
  self->socket = sock;
  return (NetListener*)self;

Error:
  if (sock != -1) close(sock);
  free(self);
  switch (errno) {
  case EADDRINUSE:
    verr_raise(VERR_NET_INUSE);
  default:
    verr_raise_system();
  }
  return NULL; // keep the compiler happy
}

static NetConn* tcp_accept(void* _self) {
  TCPListener* self = _self;
  TCPConn* conn = v_malloc(sizeof(TCPConn));

  int client = accept(self->socket, NULL, NULL);
  if (client == -1) {
    free(conn);
    switch (errno) {
    case ECONNABORTED:
    case EPROTO:
      verr_raise(VERR_NET);
    default:
      verr_raise_system();
    }
  }

  conn->base._impl = &tcp_conn_impl;
  conn->base.input = fd_input_new(client);
  conn->base.output = fd_output_new(client);
  fd_io_setclose(conn->base.input, false);
  fd_io_setclose(conn->base.output, false);
  conn->fd = client;

  return (NetConn*)conn;
}
static void tcp_close(void* _self) {
  TCPListener* self = _self;
  close(self->socket);
  free(self);
}

static NetListener_Impl tcp_listener_impl = {
  .accept = tcp_accept,
  .close = tcp_close,
};

static void tcp_conn_close(void* _self) {
  TCPConn* self = _self;
  call(self->base.input, close);
  call(self->base.output, close);
  close(self->fd);
  free(self);
}

static NetConn_Impl tcp_conn_impl = {
  .close = tcp_conn_close,
};
