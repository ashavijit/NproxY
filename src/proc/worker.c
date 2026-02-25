#include "proc/worker.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <unistd.h>

#include "core/log.h"
#include "features/access_log.h"
#include "features/metrics.h"
#include "features/rate_limit.h"
#include "http/handler.h"
#include "http/parser.h"
#include "http/request.h"
#include "http/response.h"
#include "net/conn.h"
#include "net/event_loop.h"
#include "net/timeout.h"
#include "proc/signal.h"
#include "proxy/proxy_conn.h"
#include "proxy/upstream.h"

typedef struct {
  np_socket_t *listeners;
  int listener_count;
  event_loop_t *loop;
  timeout_wheel_t *tw;
  handler_ctx_t hctx;
  conn_pool_t *pool;
  int running;
  np_config_t *cfg;
  time_t now;
} worker_state_t;

static void on_conn_timeout(void *arg) {
  conn_t *conn = (conn_t *)arg;
  worker_state_t *ws = (worker_state_t *)conn->worker_state;
  log_debug("connection timeout fd=%d", conn->fd);
  conn_pool_put(ws->pool, conn);
}

static void on_client_event(int fd, u32 events, void *arg);

static void handle_write(conn_t *conn) {
  worker_state_t *ws = (worker_state_t *)conn->worker_state;
  isize n;

  if (conn->state == CONN_SENDFILE) {
    if (buf_readable(&conn->wbuf) > 0) {
      do {
        n = buf_write_fd(&conn->wbuf, conn->fd);
      } while (n > 0);
      if (buf_readable(&conn->wbuf) > 0) {
        worker_client_event_mod(conn, EV_WRITE | EV_READ | EV_HUP | EV_EDGE);
        return;
      }
    }

    if (conn->file_remaining > 0) {
      ssize_t sent;
      do {
        sent = sendfile(conn->fd, conn->file_fd, &conn->file_offset, (size_t)conn->file_remaining);
        if (sent > 0) {
          conn->file_remaining -= sent;
        }
      } while (sent > 0 && conn->file_remaining > 0);

      if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        log_error_errno("sendfile fd=%d", conn->fd);
        conn_pool_put(ws->pool, conn);
        return;
      }
    }

    if (conn->file_remaining == 0) {
      close(conn->file_fd);
      conn->file_fd = -1;

      if (!conn->keep_alive) {
        conn_pool_put(ws->pool, conn);
        return;
      }
      arena_reset(conn->arena);
      conn->request = NULL;
      conn->response = NULL;
      conn->state = CONN_READING_REQUEST;
      worker_client_event_mod(conn, EV_READ | EV_HUP | EV_EDGE);
    } else {
      worker_client_event_mod(conn, EV_WRITE | EV_READ | EV_HUP | EV_EDGE);
    }
    return;
  }

  if (conn->state == CONN_PROXYING || conn->state == CONN_TUNNEL) {
    do {
      n = buf_write_fd(&conn->upstream_rbuf, conn->fd);
    } while (n > 0);

    if (buf_readable(&conn->upstream_rbuf) == 0) {
      event_loop_mod(conn->loop, conn->fd, EV_READ | EV_HUP | EV_EDGE, on_client_event, conn);
    } else {
      event_loop_mod(conn->loop, conn->fd, EV_WRITE | EV_READ | EV_HUP | EV_EDGE, on_client_event,
                     conn);
    }
    return;
  }

  do {
    n = buf_write_fd(&conn->wbuf, conn->fd);
  } while (n > 0);

  if (buf_readable(&conn->wbuf) == 0) {
    if (conn->state == CONN_CLOSING || !conn->keep_alive) {
      conn_pool_put(ws->pool, conn);
      return;
    }
    arena_reset(conn->arena);
    conn->request = NULL;
    conn->response = NULL;
    conn->state = CONN_READING_REQUEST;
    event_loop_mod(conn->loop, conn->fd, EV_READ | EV_HUP | EV_EDGE, on_client_event, conn);
  }
}

static void handle_read(conn_t *conn) {
  worker_state_t *ws = (worker_state_t *)conn->worker_state;
  isize n;

  if (conn->state == CONN_TUNNEL) {
    isize n;
    do {
      n = buf_read_fd(&conn->upstream_wbuf, conn->fd);
    } while (n > 0);

    log_debug("tunnel client read n=%zd readable=%zu", n, buf_readable(&conn->upstream_wbuf));

    if (n == NP_ERR_CLOSED || (n == NP_ERR && n != NP_ERR_AGAIN)) {
      worker_conn_close(conn);
      return;
    }

    if (buf_readable(&conn->upstream_wbuf) > 0) {
      do {
        n = buf_write_fd(&conn->upstream_wbuf, conn->upstream_fd);
      } while (n > 0);

      log_debug("tunnel client wrote to backend n=%zd remainder=%zu", n,
                buf_readable(&conn->upstream_wbuf));

      if (buf_readable(&conn->upstream_wbuf) == 0) {
        event_loop_mod(conn->loop, conn->upstream_fd, EV_READ | EV_HUP | EV_EDGE,
                       proxy_on_upstream_event, conn);
      } else {
        event_loop_mod(conn->loop, conn->upstream_fd, EV_WRITE | EV_READ | EV_HUP | EV_EDGE,
                       proxy_on_upstream_event, conn);
      }
    }
    return;
  }

  do {
    n = buf_read_fd(&conn->rbuf, conn->fd);
  } while (n > 0);

  if (n == NP_ERR_CLOSED) {
    conn_pool_put(ws->pool, conn);
    return;
  }
  if (n == NP_ERR && n != NP_ERR_AGAIN) {
    conn_pool_put(ws->pool, conn);
    return;
  }

  usize avail = buf_readable(&conn->rbuf);
  if (avail == 0)
    return;

  http_parse_state_t ps;
  http_parse_state_init(&ps);
  parse_result_t pr = http_parse_request(&ps, buf_read_ptr(&conn->rbuf), avail);

  if (pr == PARSE_INCOMPLETE)
    return;
  if (pr == PARSE_ERROR) {
    response_write_error(&conn->wbuf, 400, false);
    conn->state = CONN_WRITING_RESPONSE;
    event_loop_mod(conn->loop, conn->fd, EV_WRITE | EV_HUP | EV_EDGE, on_client_event, conn);
    return;
  }

  http_request_t *req = request_create(conn->arena);
  if (!req) {
    conn_pool_put(ws->pool, conn);
    return;
  }
  request_populate(req, &ps, buf_read_ptr(&conn->rbuf), avail);
  buf_consume(&conn->rbuf, ps.parsed_bytes);
  conn->keep_alive = ps.keep_alive;
  conn->request = req;

  handler_dispatch(conn, req, &ws->hctx);

  if (conn->state == CONN_PROXYING || conn->state == CONN_TUNNEL) {
    return;
  }

  isize sent;
  do {
    sent = buf_write_fd(&conn->wbuf, conn->fd);
  } while (sent > 0);

  if (buf_readable(&conn->wbuf) == 0) {
    if (!conn->keep_alive) {
      conn_pool_put(ws->pool, conn);
      return;
    }
    arena_reset(conn->arena);
    conn->request = NULL;
    conn->response = NULL;
    conn->state = CONN_READING_REQUEST;
    event_loop_mod(conn->loop, conn->fd, EV_READ | EV_HUP | EV_EDGE, on_client_event, conn);
  } else {
    conn->state = CONN_WRITING_RESPONSE;
    event_loop_mod(conn->loop, conn->fd, EV_WRITE | EV_HUP | EV_EDGE, on_client_event, conn);
  }
}

static void on_client_event(int fd, u32 events, void *arg) {
  conn_t *conn = (conn_t *)arg;
  worker_state_t *ws = (worker_state_t *)conn->worker_state;
  NP_UNUSED(fd);

  conn->last_active = ws->now;

  if (events & (EV_HUP | EPOLLERR)) {
    conn_pool_put(ws->pool, conn);
    return;
  }

  if (events & EV_READ)
    handle_read(conn);
  if (events & EV_WRITE)
    handle_write(conn);
}

static void on_accept_event(int fd, u32 events, void *arg) {
  worker_state_t *ws = (worker_state_t *)arg;
  NP_UNUSED(events);

  np_socket_t *listener = NULL;
  for (int i = 0; i < ws->listener_count; i++) {
    if (ws->listeners[i].fd == fd) {
      listener = &ws->listeners[i];
      break;
    }
  }
  if (!listener)
    return;

  for (;;) {
    struct sockaddr_in peer;
    int cfd;
    np_status_t rc = socket_accept(listener, &cfd, &peer);
    if (rc == NP_ERR_AGAIN)
      break;
    if (rc != NP_OK) {
      log_error_errno("accept");
      break;
    }

    conn_t *conn = conn_pool_get(ws->pool, cfd, &peer, ws->loop);
    if (!conn) {
      close(cfd);
      continue;
    }

    conn->worker_state = ws;
    event_loop_add(ws->loop, cfd, EV_READ | EV_HUP | EV_EDGE, on_client_event, conn);
    timeout_add(ws->tw, ws->cfg->read_timeout, on_conn_timeout, conn);
  }
}

int worker_run(np_config_t *cfg, np_socket_t *listeners, int listener_count, int worker_id) {
  log_info("worker[%d] pid=%d starting", worker_id, (int)getpid());

  worker_state_t ws;
  memset(&ws, 0, sizeof(ws));
  ws.listeners = listeners;
  ws.listener_count = listener_count;
  ws.cfg = cfg;
  ws.running = 1;
  ws.now = time(NULL);

  ws.loop = event_loop_create(NP_EPOLL_EVENTS);
  if (!ws.loop)
    return 1;

  ws.tw = timeout_wheel_create(NP_TIMEOUT_BUCKETS, 1);
  if (!ws.tw)
    return 1;

  ws.pool = conn_pool_create(4096);
  if (!ws.pool)
    return 1;

  ws.hctx.config = cfg;
  for (int i = 0; i < cfg->server_count; i++) {
    ws.hctx.upstream_pools[i] =
        cfg->servers[i].proxy.enabled ? upstream_pool_create(&cfg->servers[i]) : NULL;
  }
  ws.hctx.rate_limiter = cfg->rate_limit.enabled ? rate_limiter_create(cfg) : NULL;
  ws.hctx.metrics = cfg->metrics.enabled ? metrics_create() : NULL;

  access_log_init(cfg->log.access_log);

  signal_init(ws.loop, &ws.running);

  for (int i = 0; i < listener_count; i++) {
    event_loop_add(ws.loop, listeners[i].fd, EV_READ | EV_EDGE, on_accept_event, &ws);
  }

  event_loop_run(ws.loop, &ws.running);

  for (int i = 0; i < ws.cfg->server_count; i++) {
    if (ws.hctx.upstream_pools[i]) {
      upstream_pool_destroy(ws.hctx.upstream_pools[i]);
    }
  }
  if (ws.hctx.rate_limiter)
    rate_limiter_destroy(ws.hctx.rate_limiter);
  if (ws.hctx.metrics)
    metrics_destroy(ws.hctx.metrics);
  conn_pool_destroy(ws.pool);
  timeout_wheel_destroy(ws.tw);
  event_loop_destroy(ws.loop);
  access_log_close();

  log_info("worker[%d] exiting", worker_id);
  return 0;
}

void worker_conn_close(conn_t *conn) {
  worker_state_t *ws = (worker_state_t *)conn->worker_state;
  conn_pool_put(ws->pool, conn);
}

void worker_client_event_mod(conn_t *conn, u32 events) {
  event_loop_mod(conn->loop, conn->fd, events, on_client_event, conn);
}
