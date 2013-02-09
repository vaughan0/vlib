
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
  NetListener base;
  int socket;
};

static NetListener_Impl tcp_listener_impl;

data(TCPConn) {
  NetConn base;
  int     fd;
};

static NetConn_Impl tcp_conn_impl;

// Performs a lookup given an address (node:service).
static struct addrinfo* lookup(const char* addr, bool passive) {
  // Split addr into node and service
  const char *node, *service;
  char* index = strchr(addr, ':');
  char buf[256];
  if (index) {
    int nodelen = (int)(index - addr);
    if (nodelen+1 > sizeof(buf)) RAISE(ARGUMENT);
    node = buf;
    memcpy(buf, addr, nodelen);
    buf[nodelen] = 0;
    service = index+1;
  } else {
    node = addr;
    service = NULL;
  }
  if (node[0] == '*' && node[1] == 0)
    node = NULL;
  // Perform the lookup
  struct addrinfo hints = {
    .ai_socktype = SOCK_STREAM,
    .ai_flags = passive ? AI_PASSIVE : 0,
  };
  struct addrinfo* result;
  int r = getaddrinfo(node, service, &hints, &result);
  if (r) RAISE(NET_LOOKUP);
  return result;
}

// Binds an IPv4 or IPv6 socket, depending on the lookup results
static int bind_socket(struct addrinfo* lookup) {
  int sock, r;
  struct addrinfo* tofree = lookup;
  do {
    switch (lookup->ai_family) {
    case AF_INET:
    case AF_INET6:
      goto Bind;
    }
    lookup = lookup->ai_next;
    if (!lookup) return -1;
  } while (true);

Bind:
  sock = socket(lookup->ai_family, SOCK_STREAM, 0);
  if (sock == -1) goto Error;
  r = bind(sock, lookup->ai_addr, lookup->ai_addrlen);
  if (r == -1) goto Error;
  int reuse = 1;
  r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (r == -1) goto Error;

  freeaddrinfo(tofree);
  return sock;

Error:
  r = errno;
  freeaddrinfo(tofree);
  if (sock != -1) close(sock);
  if (r == EADDRINUSE) {
    RAISE(NET_INUSE);
  } else {
    verr_raise(verr_system(r));
  }
  return 0;
}

NetListener* net_listen_tcp(const char* addr) {
  struct addrinfo* result = lookup(addr, true);
  int sock = bind_socket(result);
  int r = listen(sock, 5);
  if (r == -1) {
    close(sock);
    verr_raise_system();
  }

  TCPListener* self = v_malloc(sizeof(TCPListener));
  self->base._impl = &tcp_listener_impl;
  self->socket = sock;
  return &self->base;
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
  call(self->base.input, close, false);
  call(self->base.output, close, false);
  close(self->fd);
  free(self);
}

static NetConn_Impl tcp_conn_impl = {
  .close = tcp_conn_close,
};
