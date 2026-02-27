#ifndef NPROXY_CONN_H
#define NPROXY_CONN_H

#include <netinet/in.h>
#include <time.h>

#include "core/memory.h"
#include "core/types.h"
#include "net/buffer.h"

typedef enum {
  CONN_READING_REQUEST = 0,
  CONN_WRITING_RESPONSE,
  CONN_PROXYING = 2,
  CONN_TUNNEL = 3,
  CONN_CLOSING = 4,
  CONN_SENDFILE = 5,
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
  int file_fd;
  off_t file_offset;
  off_t file_remaining;
  bool keep_alive;
  bool tls;
  void *proxy_pool;
  void *tls_conn;
  void *request;
  void *response;
  event_loop_t *loop;
  void *worker_state;
  void *cache_store;
  char cache_key[256];
  u8 *cache_buf;
  usize cache_len;
  usize cache_cap;
  int proxy_status;
  conn_t *next;
  conn_t *prev;
};

typedef struct {
  conn_t *free_head;
  int free_count;
  int max_free;
} conn_pool_t;

conn_pool_t *conn_pool_create(int max_free);
void conn_pool_destroy(conn_pool_t *pool);
conn_t *conn_pool_get(conn_pool_t *pool, int fd, struct sockaddr_in *peer, event_loop_t *loop);
void conn_pool_put(conn_pool_t *pool, conn_t *conn);

conn_t *conn_create(int fd, struct sockaddr_in *peer, event_loop_t *loop);
void conn_destroy(conn_t *conn);
void conn_close(conn_t *conn);
np_status_t conn_set_upstream(conn_t *conn, int upstream_fd);

#endif
