#include "http/handler.h"

#include <arpa/inet.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "core/string_util.h"
#include "features/access_log.h"
#include "features/health.h"
#include "features/metrics.h"
#include "features/rate_limit.h"
#include "http/response.h"
#include "module/module.h"
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

  np_server_config_t *server = &ctx->config->servers[0];
  str_t host_hdr = request_header(req, STR("Host"));
  if (host_hdr.ptr) {
    char host_buf[256] = {0};
    usize copy_len = host_hdr.len < 255 ? host_hdr.len : 255;
    memcpy(host_buf, host_hdr.ptr, copy_len);

    char *colon = strchr(host_buf, ':');
    if (colon)
      *colon = '\0';

    for (int i = 0; i < ctx->config->server_count; i++) {
      if (ctx->config->servers[i].server_name[0] != '\0' &&
          strcmp(ctx->config->servers[i].server_name, host_buf) == 0) {
        server = &ctx->config->servers[i];
        break;
      }
    }
  }

  // We temporarily cast server to config to avoid breaking module interfaces
  if (module_run_request_handlers(conn, req, ctx->config) == NP_MODULE_HANDLED) {
    access_log_write(req, 200, 0, &start);
    return;
  }

  for (int i = 0; i < server->rewrite.count; i++) {
    rewrite_rule_t *rule = &server->rewrite.rules[i];
    regmatch_t matches[10];
    char path_buf[1024];
    usize plen = req->path.len < sizeof(path_buf) - 1 ? req->path.len : sizeof(path_buf) - 1;
    memcpy(path_buf, req->path.ptr, plen);
    path_buf[plen] = '\0';

    if (regexec(&rule->re, path_buf, 10, matches, 0) == 0) {
      char new_path[1024] = {0};
      char *dest = new_path;
      char *src = rule->replacement;
      while (*src && (dest - new_path) < (isize)sizeof(new_path) - 1) {
        if (*src == '$' && *(src + 1) >= '0' && *(src + 1) <= '9') {
          int idx = *(src + 1) - '0';
          if (matches[idx].rm_so != -1) {
            int len = matches[idx].rm_eo - matches[idx].rm_so;
            if (len > 0 && (dest - new_path + len) < (isize)sizeof(new_path) - 1) {
              memcpy(dest, path_buf + matches[idx].rm_so, len);
              dest += len;
            }
          }
          src += 2;
        } else {
          *dest++ = *src++;
        }
      }
      *dest = '\0';
      usize nlen = strlen(new_path);
      char *p = arena_alloc(conn->arena, nlen + 1);
      if (p) {
        memcpy(p, new_path, nlen + 1);
        req->path.ptr = p;
        req->path.len = nlen;
      }
      break;
    }
  }

  if (ctx->rate_limiter) {
    np_status_t rl = rate_limit_check(ctx->rate_limiter, req->remote_ip);
    if (rl != NP_OK) {
      response_write_error(&conn->wbuf, 429, req->keep_alive);
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

  int s_idx = server - ctx->config->servers;
  if (server->proxy.enabled && ctx->upstream_pools[s_idx]) {
    // Note: proxy_handle expects ctx, we'll need to patch proxy_conn to use server explicitly
    // later. For now, upstream_pool array mapping needs to happen outside this.
    proxy_handle(conn, req, ctx, server, ctx->upstream_pools[s_idx]);
    access_log_write(req, 0, 0, &start);
    return;
  }

  if (server->static_root[0] != '\0') {
    int status = file_server_handle(conn, req, server);
    access_log_write(req, status, 0, &start);
    return;
  }

  response_write_error(&conn->wbuf, 404, req->keep_alive);
  metrics_inc_requests(ctx->metrics, 404);
  access_log_write(req, 404, 0, &start);
  conn->state = CONN_WRITING_RESPONSE;
}
