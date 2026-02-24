#include "proxy/proxy_conn.h"

#include <stdio.h>
#include <string.h>

#include "core/log.h"
#include "features/metrics.h"
#include "http/response.h"
#include "net/event_loop.h"
#include "net/socket.h"
#include "proxy/upstream.h"

static void send_error(conn_t *conn, int status, const char *msg) {
  response_write_simple(&conn->wbuf, status, NULL, "text/plain", msg, false);
  conn->state = CONN_WRITING_RESPONSE;
}

static void build_proxy_request(conn_t *conn, http_request_t *req) {
  char buf[NP_MAX_HEADER_LEN];
  int n = snprintf(buf, sizeof(buf),
                   "%s " STR_FMT
                   " HTTP/1.1\r\n"
                   "Host: " STR_FMT
                   "\r\n"
                   "X-Real-IP: %s\r\n"
                   "X-Forwarded-For: %s\r\n"
                   "Connection: close\r\n",
                   http_method_str(req->method), STR_ARG(req->path),
                   STR_ARG(request_header(req, STR("Host"))), req->remote_ip, req->remote_ip);

  for (int i = 0; i < req->header_count; i++) {
    str_t name = req->headers[i].name;
    if (str_ieq(name, STR("Host")) || str_ieq(name, STR("Connection"))) continue;
    int hn = snprintf(buf + n, sizeof(buf) - (usize)n, STR_FMT ": " STR_FMT "\r\n", STR_ARG(name),
                      STR_ARG(req->headers[i].value));
    if (hn > 0) n += hn;
  }

  n += snprintf(buf + n, sizeof(buf) - (usize)n, "\r\n");

  if (n > 0 && buf_writable(&conn->upstream_wbuf) >= (usize)n) {
    memcpy(buf_write_ptr(&conn->upstream_wbuf), buf, (usize)n);
    buf_produce(&conn->upstream_wbuf, (usize)n);
  }

  if (req->body.len > 0 && buf_writable(&conn->upstream_wbuf) >= req->body.len) {
    memcpy(buf_write_ptr(&conn->upstream_wbuf), req->body.ptr, req->body.len);
    buf_produce(&conn->upstream_wbuf, req->body.len);
  }
}

static int req_keep_alive_check(conn_t *conn) {
  NP_UNUSED(conn);
  return 0;
}

void proxy_on_upstream_event(int fd, u32 events, void *arg) {
  conn_t *conn = (conn_t *)arg;

  if (events & (EV_HUP | EPOLLERR)) {
    event_loop_del(conn->loop, fd);
    if (buf_readable(&conn->upstream_rbuf) == 0) {
      send_error(conn, 502, "bad gateway\n");
    }
    conn->state = CONN_WRITING_RESPONSE;
    buf_write_fd(&conn->wbuf, conn->fd);
    return;
  }

  if (events & EV_WRITE) {
    isize n = buf_write_fd(&conn->upstream_wbuf, fd);
    if (n < 0 && n != NP_ERR_AGAIN) {
      send_error(conn, 502, "bad gateway\n");
      return;
    }
    if (buf_readable(&conn->upstream_wbuf) == 0) {
      event_loop_mod(conn->loop, fd, EV_READ | EV_HUP | EV_EDGE, proxy_on_upstream_event, conn);
    }
  }

  if (events & EV_READ) {
    isize n;
    do {
      n = buf_read_fd(&conn->upstream_rbuf, fd);
      if (n > 0) {
        buf_write_fd(&conn->upstream_rbuf, conn->fd);
      }
    } while (n > 0);

    if (n == NP_ERR_CLOSED) {
      event_loop_del(conn->loop, fd);
      conn->state = req_keep_alive_check(conn) ? CONN_READING_REQUEST : CONN_CLOSING;
    }
  }
}

void proxy_handle(conn_t *conn, http_request_t *req, handler_ctx_t *ctx) {
  upstream_pool_t *pool = (upstream_pool_t *)ctx->upstream_pool;
  upstream_backend_t *be = upstream_select(pool);

  if (!be) {
    response_write_simple(&conn->wbuf, 503, "Service Unavailable", "text/plain",
                          "no healthy backends\n", req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    metrics_inc_upstream_errors(ctx->metrics);
    return;
  }

  int ufd;
  if (socket_connect_nonblock(&ufd, be->host, be->port) != NP_OK) {
    upstream_release(pool, be, true);
    response_write_simple(&conn->wbuf, 502, "Bad Gateway", "text/plain",
                          "upstream connect failed\n", req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    metrics_inc_upstream_errors(ctx->metrics);
    return;
  }

  conn_set_upstream(conn, ufd);
  build_proxy_request(conn, req);
  conn->state = CONN_PROXYING;

  event_loop_add(conn->loop, ufd, EV_WRITE | EV_READ | EV_HUP | EV_EDGE, proxy_on_upstream_event,
                 conn);
}
