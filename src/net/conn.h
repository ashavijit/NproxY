#ifndef NPROXY_CONN_H
#define NPROXY_CONN_H

#include "core/memory.h"
#include "core/types.h"
#include "net/buffer.h"
#include <netinet/in.h>
#include <time.h>

typedef enum {
  CONN_READING_REQUEST = 0,
  CONN_WRITING_RESPONSE,
  CONN_PROXYING,
  CONN_CLOSING,
} conn_state_t;

typedef struct conn conn_t;
typedef struct event_loop event_loop_t;

struct conn {
  int fd;
  int upstream_fd;
  conn_state_t state;
  arena_t *arena;
  np_buf_t rbuf;
  np_buf_t wbuf;
  np_buf_t upstream_rbuf;
  np_buf_t upstream_wbuf;
  struct sockaddr_in peer;
  time_t last_active;
  bool keep_alive;
  bool tls;
  void *tls_conn;
  void *request;
  void *response;
  event_loop_t *loop;
  conn_t *next;
  conn_t *prev;
};

conn_t *conn_create(int fd, struct sockaddr_in *peer, event_loop_t *loop);
void conn_destroy(conn_t *conn);
void conn_close(conn_t *conn);
np_status_t conn_set_upstream(conn_t *conn, int upstream_fd);

#endif
