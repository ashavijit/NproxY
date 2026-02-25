#include "net/conn.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "net/event_loop.h"

static void conn_init_bufs(conn_t *c) {
  buf_reset(&c->rbuf);
  buf_reset(&c->wbuf);
  buf_reset(&c->upstream_rbuf);
  buf_reset(&c->upstream_wbuf);
}

static conn_t *conn_alloc(void) {
  conn_t *c = malloc(sizeof(*c));
  if (!c)
    return NULL;
  memset(c, 0, sizeof(*c));

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

static void conn_reset(conn_t *c, int fd, struct sockaddr_in *peer, event_loop_t *loop) {
  c->fd = fd;
  c->upstream_fd = -1;
  c->file_fd = -1;
  c->file_offset = 0;
  c->file_remaining = 0;
  c->state = CONN_READING_REQUEST;
  c->loop = loop;
  c->last_active = 0;
  c->keep_alive = false;
  c->tls = false;
  c->tls_conn = NULL;
  c->request = NULL;
  c->response = NULL;
  c->next = NULL;
  c->prev = NULL;
  if (peer)
    c->peer = *peer;
  arena_reset(c->arena);
  conn_init_bufs(c);
}

conn_pool_t *conn_pool_create(int max_free) {
  conn_pool_t *p = malloc(sizeof(*p));
  if (!p)
    return NULL;
  p->free_head = NULL;
  p->free_count = 0;
  p->max_free = max_free;
  return p;
}

void conn_pool_destroy(conn_pool_t *pool) {
  conn_t *c = pool->free_head;
  while (c) {
    conn_t *next = c->next;
    buf_free(&c->rbuf);
    buf_free(&c->wbuf);
    buf_free(&c->upstream_rbuf);
    buf_free(&c->upstream_wbuf);
    arena_destroy(c->arena);
    free(c);
    c = next;
  }
  free(pool);
}

conn_t *conn_pool_get(conn_pool_t *pool, int fd, struct sockaddr_in *peer, event_loop_t *loop) {
  conn_t *c = NULL;
  if (pool->free_head) {
    c = pool->free_head;
    pool->free_head = c->next;
    pool->free_count--;
  } else {
    c = conn_alloc();
  }
  if (!c)
    return NULL;
  conn_reset(c, fd, peer, loop);
  return c;
}

void conn_pool_put(conn_pool_t *pool, conn_t *conn) {
  conn_close(conn);
  if (pool->free_count >= pool->max_free) {
    buf_free(&conn->rbuf);
    buf_free(&conn->wbuf);
    buf_free(&conn->upstream_rbuf);
    buf_free(&conn->upstream_wbuf);
    arena_destroy(conn->arena);
    free(conn);
    return;
  }
  arena_reset(conn->arena);
  conn_init_bufs(conn);
  conn->next = pool->free_head;
  pool->free_head = conn;
  pool->free_count++;
}

conn_t *conn_create(int fd, struct sockaddr_in *peer, event_loop_t *loop) {
  conn_t *c = conn_alloc();
  if (!c)
    return NULL;
  conn_reset(c, fd, peer, loop);
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
  if (conn->file_fd >= 0) {
    close(conn->file_fd);
    conn->file_fd = -1;
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
