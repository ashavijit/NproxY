#include "net/socket.h"
#include "core/log.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

np_status_t socket_set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    return NP_ERR;
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    return NP_ERR;
  return NP_OK;
}

np_status_t socket_create_listener(np_socket_t *sock, const char *host,
                                   u16 port, int backlog) {
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) {
    log_error_errno("socket_create_listener: socket()");
    return NP_ERR;
  }

  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

  int rcvbuf = 256 * 1024;
  int sndbuf = 256 * 1024;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (!host || strcmp(host, "0.0.0.0") == 0) {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
      log_error("socket_create_listener: invalid address %s", host);
      close(fd);
      return NP_ERR;
    }
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    log_error_errno("socket_create_listener: bind()");
    close(fd);
    return NP_ERR;
  }

  if (listen(fd, backlog) < 0) {
    log_error_errno("socket_create_listener: listen()");
    close(fd);
    return NP_ERR;
  }

  sock->fd = fd;
  sock->addr = addr;
  sock->addr_len = sizeof(addr);
  log_info("listening on %s:%d", host ? host : "0.0.0.0", port);
  return NP_OK;
}

np_status_t socket_accept(np_socket_t *listener, int *client_fd,
                          struct sockaddr_in *peer) {
  socklen_t len = sizeof(*peer);
  int fd = accept4(listener->fd, (struct sockaddr *)peer, &len,
                   SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return NP_ERR_AGAIN;
    return NP_ERR;
  }

  int opt = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

  int keepalive = 1, keepidle = 60, keepintvl = 10, keepcnt = 3;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
  setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

  *client_fd = fd;
  return NP_OK;
}

np_status_t socket_connect_nonblock(int *fd_out, const char *host, u16 port) {
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return NP_ERR;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    close(fd);
    return NP_ERR;
  }

  int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0 && errno != EINPROGRESS) {
    close(fd);
    return NP_ERR;
  }

  *fd_out = fd;
  return NP_OK;
}

void socket_close(int fd) {
  if (fd >= 0)
    close(fd);
}
