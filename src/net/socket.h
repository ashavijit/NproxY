#ifndef NPROXY_SOCKET_H
#define NPROXY_SOCKET_H

#include "core/types.h"
#include <netinet/in.h>

typedef struct {
  int fd;
  struct sockaddr_in addr;
  socklen_t addr_len;
} np_socket_t;

np_status_t socket_create_listener(np_socket_t *sock, const char *host,
                                   u16 port, int backlog);
np_status_t socket_connect_nonblock(int *fd_out, const char *host, u16 port);
np_status_t socket_set_nonblocking(int fd);
np_status_t socket_accept(np_socket_t *listener, int *client_fd,
                          struct sockaddr_in *peer);
void socket_close(int fd);

#endif
