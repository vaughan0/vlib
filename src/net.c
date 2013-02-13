
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

data(TCPConn) {
  NetConn base;
  int     fd;
};

static void tcp_conn_close(void* _self) {
  TCPConn* self = _self;
  unclosable_input_close(self->base.input);
  unclosable_output_close(self->base.output);
  close(self->fd);
  free(self);
}
static NetConn_Impl tcp_conn_impl = {
  .close = tcp_conn_close,
};

static NetConn* tcp_conn_new(int fd) {
  TCPConn* self;
  TRY {
    self = v_malloc(sizeof(TCPConn));
    self->base._impl = &tcp_conn_impl;
    self->base.input = unclosable_input_new(fd_input_new(fd, false));
    self->base.output = unclosable_output_new(fd_output_new(fd, false));
    self->fd = fd;
  } CATCH(err) {
    close(fd);
    verr_raise(err);
  } ETRY
  return &self->base;
}

data(TCPListener) {
  NetListener base;
  int socket;
};

static NetListener_Impl tcp_listener_impl;

NetListener* net_listen_tcp(const char* addr) {
  int eno;
  struct addrinfo* result = lookup(addr, true);

  int sock = socket(result->ai_family, SOCK_STREAM, 0);
  if (sock == -1) goto Error;
  int reuse = 1;
  int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (r == -1) goto Error;
  r = bind(sock, result->ai_addr, result->ai_addrlen);
  if (r == -1) goto Error;
  r = listen(sock, 5);
  if (r == -1) goto Error;

  // Create TCPListener
  freeaddrinfo(result);
  TCPListener* self;
  TRY {
    self = v_malloc(sizeof(TCPListener));
  } CATCH(err) {
    close(sock);
    verr_raise(err);
  } ETRY

  self->base._impl = &tcp_listener_impl;
  self->socket = sock;
  return &self->base;

Error:
  eno = errno;
  freeaddrinfo(result);
  if (sock != -1) close(sock);
  verr_raise(verr_system(eno));
  return NULL;
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

  return tcp_conn_new(client);
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

NetConn* net_connect_tcp(const char* addr, const char* bindto) {
  int eno;

  // Lookup remote and local addresses
  struct addrinfo* remote = lookup(addr, false);
  struct addrinfo* bindaddr = NULL;
  struct addrinfo* bindaddr_free = NULL;
  if (bindto) {
    TRY {
      bindaddr = lookup(bindto, false);
      bindaddr_free = bindaddr;
      for (;;) {
        if (bindaddr->ai_family == remote->ai_family) break;
        bindaddr = bindaddr->ai_next;
        if (!bindaddr) RAISE(NET_LOOKUP);
      }
    } CATCH(err) {
      if (bindaddr_free) freeaddrinfo(bindaddr_free);
      freeaddrinfo(remote);
      verr_raise(err);
    } ETRY
  }

  // Create socket
  int sock = socket(remote->ai_family, SOCK_STREAM, 0);
  if (sock == -1) goto Error;
  if (bindaddr) {
    // Set SO_REUSEADDR
    int reuse = 1;
    int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (r == -1) goto Error;
    // Bind
    r = bind(sock, bindaddr->ai_addr, bindaddr->ai_addrlen);
    if (r == -1) goto Error;
  }
  // Connect
  int r = connect(sock, remote->ai_addr, remote->ai_addrlen);
  if (r == -1) goto Error;

  freeaddrinfo(remote);
  if (bindaddr_free) freeaddrinfo(bindaddr_free);
  return tcp_conn_new(sock);

Error:
  eno = errno;
  if (sock != -1) close(sock);
  freeaddrinfo(remote);
  if (bindaddr_free) freeaddrinfo(bindaddr_free);
  verr_raise(verr_system(eno));
  return NULL;
}
