#include "http/handler.h"

#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "core/string_util.h"
#include "features/access_log.h"
#include "features/health.h"
#include "features/metrics.h"
#include "features/rate_limit.h"
#include "http/response.h"
#include "proxy/proxy_conn.h"
#include "static/file_server.h"

static bool path_matches(str_t path, const char *prefix) {
  str_t p = STR_NULL;
  p.ptr = prefix;
  p.len = strlen(prefix);
  return str_starts_with(path, p);
}

void handler_dispatch(conn_t *conn, http_request_t *req, handler_ctx_t *ctx) {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);

  inet_ntop(AF_INET, &conn->peer.sin_addr, req->remote_ip, sizeof(req->remote_ip));

  if (ctx->rate_limiter) {
    np_status_t rl = rate_limit_check(ctx->rate_limiter, req->remote_ip);
    if (rl != NP_OK) {
      response_write_simple(&conn->wbuf, 429, "Too Many Requests", "text/plain",
                            "rate limit exceeded\n", req->keep_alive);
      metrics_inc_requests(ctx->metrics, 429);
      access_log_write(req, 429, 0, &start);
      conn->state = CONN_WRITING_RESPONSE;
      return;
    }
  }

  if (ctx->metrics && path_matches(req->path, ctx->config->metrics.path)) {
    metrics_handle(ctx->metrics, conn, req);
    access_log_write(req, 200, 0, &start);
    return;
  }

  if (path_matches(req->path, "/healthz")) {
    health_handle(conn, req);
    access_log_write(req, 200, 0, &start);
    return;
  }

  if (ctx->config->proxy.enabled && ctx->upstream_pool) {
    proxy_handle(conn, req, ctx);
    access_log_write(req, 0, 0, &start);
    return;
  }

  if (ctx->config->static_root[0] != '\0') {
    int status = file_server_handle(conn, req, ctx->config->static_root);
    access_log_write(req, status, 0, &start);
    return;
  }

  response_write_simple(&conn->wbuf, 404, "Not Found", "text/plain", "not found\n",
                        req->keep_alive);
  metrics_inc_requests(ctx->metrics, 404);
  access_log_write(req, 404, 0, &start);
  conn->state = CONN_WRITING_RESPONSE;
}
