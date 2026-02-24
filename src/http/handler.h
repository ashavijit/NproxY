#ifndef NPROXY_HANDLER_H
#define NPROXY_HANDLER_H

#include "core/config.h"
#include "core/types.h"
#include "http/request.h"
#include "net/conn.h"

typedef struct handler_ctx handler_ctx_t;

struct handler_ctx {
  np_config_t *config;
  void *upstream_pool;
  void *rate_limiter;
  void *metrics;
};

void handler_dispatch(conn_t *conn, http_request_t *req, handler_ctx_t *ctx);

#endif
