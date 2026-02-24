#include "net/conn.h"
#include "core/log.h"
#include "net/event_loop.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

conn_t *conn_create(int fd, struct sockaddr_in *peer, event_loop_t *loop) {
  conn_t *c = malloc(sizeof(*c));
  if (!c)
    return NULL;
  memset(c, 0, sizeof(*c));

  c->fd = fd;
  c->upstream_fd = -1;
  c->state = CONN_READING_REQUEST;
  c->loop = loop;
  c->last_active = time(NULL);
  c->keep_alive = false;
  c->tls = false;
  c->tls_conn = NULL;
  c->request = NULL;
  c->response = NULL;
  c->next = NULL;
  c->prev = NULL;
  if (peer)
    c->peer = *peer;

  c->arena = arena_create(NP_ARENA_SIZE);
  if (!c->arena) {
    free(c);
    return NULL;
  }

  if (buf_init(&c->rbuf, NP_READ_BUF_SIZE) != NP_OK ||
      buf_init(&c->wbuf, NP_WRITE_BUF_SIZE) != NP_OK ||
      buf_init(&c->upstream_rbuf, NP_READ_BUF_SIZE) != NP_OK ||
      buf_init(&c->upstream_wbuf, NP_WRITE_BUF_SIZE) != NP_OK) {
    arena_destroy(c->arena);
    free(c);
    return NULL;
  }
  return c;
}

np_status_t conn_set_upstream(conn_t *conn, int upstream_fd) {
  conn->upstream_fd = upstream_fd;
  return NP_OK;
}

void conn_close(conn_t *conn) {
  if (conn->fd >= 0) {
    event_loop_del(conn->loop, conn->fd);
    close(conn->fd);
    conn->fd = -1;
  }
  if (conn->upstream_fd >= 0) {
    event_loop_del(conn->loop, conn->upstream_fd);
    close(conn->upstream_fd);
    conn->upstream_fd = -1;
  }
  conn->state = CONN_CLOSING;
}

void conn_destroy(conn_t *conn) {
  conn_close(conn);
  buf_free(&conn->rbuf);
  buf_free(&conn->wbuf);
  buf_free(&conn->upstream_rbuf);
  buf_free(&conn->upstream_wbuf);
  arena_destroy(conn->arena);
  free(conn);
}
