#ifndef NPROXY_HEALTH_H
#define NPROXY_HEALTH_H

#include "http/request.h"
#include "net/conn.h"

void health_handle(conn_t *conn, http_request_t *req);

#endif
