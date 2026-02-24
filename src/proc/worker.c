#include "proc/worker.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
#include "proxy/upstream.h"

typedef struct {
  np_socket_t *listener;
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
  if (avail == 0) return;

  http_parse_state_t ps;
  http_parse_state_init(&ps);
  parse_result_t pr = http_parse_request(&ps, buf_read_ptr(&conn->rbuf), avail);

  if (pr == PARSE_INCOMPLETE) return;
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

  if (conn->state == CONN_PROXYING) {
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

  if (events & EV_READ) handle_read(conn);
  if (events & EV_WRITE) handle_write(conn);
}

static void on_accept_event(int fd, u32 events, void *arg) {
  worker_state_t *ws = (worker_state_t *)arg;
  NP_UNUSED(fd);
  NP_UNUSED(events);

  for (;;) {
    struct sockaddr_in peer;
    int cfd;
    np_status_t rc = socket_accept(ws->listener, &cfd, &peer);
    if (rc == NP_ERR_AGAIN) break;
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

int worker_run(np_config_t *cfg, np_socket_t *listener, int worker_id) {
  log_info("worker[%d] pid=%d starting", worker_id, (int)getpid());

  worker_state_t ws;
  memset(&ws, 0, sizeof(ws));
  ws.listener = listener;
  ws.cfg = cfg;
  ws.running = 1;
  ws.now = time(NULL);

  ws.loop = event_loop_create(NP_EPOLL_EVENTS);
  if (!ws.loop) return 1;

  ws.tw = timeout_wheel_create(NP_TIMEOUT_BUCKETS, 1);
  if (!ws.tw) return 1;

  ws.pool = conn_pool_create(4096);
  if (!ws.pool) return 1;

  ws.hctx.config = cfg;
  ws.hctx.upstream_pool = cfg->proxy.enabled ? upstream_pool_create(cfg) : NULL;
  ws.hctx.rate_limiter = cfg->rate_limit.enabled ? rate_limiter_create(cfg) : NULL;
  ws.hctx.metrics = cfg->metrics.enabled ? metrics_create() : NULL;

  access_log_init(cfg->log.access_log);

  signal_init(ws.loop, &ws.running);

  event_loop_add(ws.loop, listener->fd, EV_READ | EV_EDGE, on_accept_event, &ws);

  event_loop_run(ws.loop, &ws.running);

  if (ws.hctx.upstream_pool) upstream_pool_destroy(ws.hctx.upstream_pool);
  if (ws.hctx.rate_limiter) rate_limiter_destroy(ws.hctx.rate_limiter);
  if (ws.hctx.metrics) metrics_destroy(ws.hctx.metrics);
  conn_pool_destroy(ws.pool);
  timeout_wheel_destroy(ws.tw);
  event_loop_destroy(ws.loop);
  access_log_close();

  log_info("worker[%d] exiting", worker_id);
  return 0;
}
