#ifndef NPROXY_METRICS_H
#define NPROXY_METRICS_H

#include "core/types.h"
#include "http/request.h"
#include "net/conn.h"

typedef struct np_metrics np_metrics_t;

np_metrics_t *metrics_create(void);
void metrics_destroy(np_metrics_t *m);
void metrics_inc_requests(np_metrics_t *m, int status);
void metrics_inc_active(np_metrics_t *m);
void metrics_dec_active(np_metrics_t *m);
void metrics_inc_upstream_errors(np_metrics_t *m);
void metrics_observe_latency(np_metrics_t *m, u64 latency_us);
void metrics_handle(np_metrics_t *m, conn_t *conn, http_request_t *req);

#endif
