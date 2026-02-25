#include "proxy/proxy_conn.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "features/metrics.h"
#include "http/response.h"
#include "net/event_loop.h"
#include "net/socket.h"
#include "proc/worker.h"
#include "proxy/upstream.h"

static void send_error(conn_t *conn, int status) {
  response_write_error(&conn->wbuf, status, false);
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
                   "Connection: %s\r\n",
                   http_method_str(req->method), STR_ARG(req->path),
                   STR_ARG(request_header(req, STR("Host"))), req->remote_ip, req->remote_ip,
                   req->upgrade ? "Upgrade" : "keep-alive");

  for (int i = 0; i < req->header_count; i++) {
    str_t name = req->headers[i].name;
    if (str_ieq(name, STR("Host")) || str_ieq(name, STR("Connection")))
      continue;
    int hn = snprintf(buf + n, sizeof(buf) - (usize)n, STR_FMT ": " STR_FMT "\r\n", STR_ARG(name),
                      STR_ARG(req->headers[i].value));
    if (hn > 0)
      n += hn;
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

void proxy_on_upstream_event(int fd, u32 events, void *arg) {
  conn_t *conn = (conn_t *)arg;

  if (events & EV_WRITE) {
    isize n = buf_write_fd(&conn->upstream_wbuf, fd);
    if (n < 0 && n != NP_ERR_AGAIN) {
      send_error(conn, 502);
      buf_write_fd(&conn->wbuf, conn->fd);
      worker_conn_close(conn);
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
        isize wn;
        do {
          wn = buf_write_fd(&conn->upstream_rbuf, conn->fd);
        } while (wn > 0);
      }
    } while (n > 0);

    if (buf_readable(&conn->upstream_rbuf) > 0) {
      worker_client_event_mod(conn, EV_WRITE | EV_READ | EV_HUP | EV_EDGE);
    }

    /* If read returns error and we didn't send anything yet, could be a 502.
     * However, the connection is closed. We just close cleanly here. */
    if (n == NP_ERR_CLOSED || n == NP_ERR) {
      event_loop_del(conn->loop, fd);
      conn->upstream_fd = -1;  // Unlink so worker_conn_close doesn't double close

      upstream_pool_t *pool =
          (upstream_pool_t *)((handler_ctx_t *)conn->worker_state)->upstream_pool;

      if (n == NP_ERR_CLOSED && conn->state != CONN_TUNNEL) {
        upstream_backend_t *be = (upstream_backend_t *)conn->tls_conn;
        if (be) {
          upstream_put_connection(pool, be, fd);
          worker_conn_close(conn);
          return;
        }
      }

      close(fd);
      worker_conn_close(conn);
    }
  }
}

void proxy_handle(conn_t *conn, http_request_t *req, handler_ctx_t *ctx) {
  upstream_pool_t *pool = (upstream_pool_t *)ctx->upstream_pool;
  upstream_backend_t *be = upstream_select(pool);

  if (!be) {
    response_write_error(&conn->wbuf, 503, req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    metrics_inc_upstream_errors(ctx->metrics);
    return;
  }

  int ufd = upstream_get_connection(pool, be);
  if (ufd < 0) {
    upstream_release(pool, be, true);
    response_write_error(&conn->wbuf, 502, req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    metrics_inc_upstream_errors(ctx->metrics);
    return;
  }

  conn->tls_conn = be;

  conn_set_upstream(conn, ufd);
  build_proxy_request(conn, req);
  conn->state = req->upgrade ? CONN_TUNNEL : CONN_PROXYING;

  event_loop_add(conn->loop, ufd, EV_WRITE | EV_READ | EV_HUP | EV_EDGE, proxy_on_upstream_event,
                 conn);
}
