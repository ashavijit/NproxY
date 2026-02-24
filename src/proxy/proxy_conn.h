#ifndef NPROXY_PROXY_CONN_H
#define NPROXY_PROXY_CONN_H

#include "core/types.h"
#include "http/handler.h"
#include "http/request.h"
#include "net/conn.h"

void proxy_handle(conn_t *conn, http_request_t *req, handler_ctx_t *ctx);
void proxy_on_upstream_event(int fd, u32 events, void *arg);

#endif
