#include "features/metrics.h"
#include "core/types.h"
#include "http/response.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIST_BUCKETS 16

struct np_metrics {
  _Atomic u64 requests_total;
  _Atomic u64 requests_2xx;
  _Atomic u64 requests_4xx;
  _Atomic u64 requests_5xx;
  _Atomic u64 active_connections;
  _Atomic u64 upstream_errors;
  _Atomic u64 latency_hist[HIST_BUCKETS];
  _Atomic u64 latency_sum_us;
  _Atomic u64 latency_count;
};

static const u64 hist_bounds[] = {
    100,    500,    1000,   2000,    5000,    10000,   20000,    50000,
    100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000, UINT64_MAX};

np_metrics_t *metrics_create(void) {
  np_metrics_t *m = calloc(1, sizeof(*m));
  return m;
}

void metrics_destroy(np_metrics_t *m) { free(m); }

void metrics_inc_requests(np_metrics_t *m, int status) {
  if (!m)
    return;
  atomic_fetch_add(&m->requests_total, 1);
  if (status >= 200 && status < 300)
    atomic_fetch_add(&m->requests_2xx, 1);
  else if (status >= 400 && status < 500)
    atomic_fetch_add(&m->requests_4xx, 1);
  else if (status >= 500)
    atomic_fetch_add(&m->requests_5xx, 1);
}

void metrics_inc_active(np_metrics_t *m) {
  if (m)
    atomic_fetch_add(&m->active_connections, 1);
}

void metrics_dec_active(np_metrics_t *m) {
  if (m)
    atomic_fetch_sub(&m->active_connections, 1);
}

void metrics_inc_upstream_errors(np_metrics_t *m) {
  if (m)
    atomic_fetch_add(&m->upstream_errors, 1);
}

void metrics_observe_latency(np_metrics_t *m, u64 latency_us) {
  if (!m)
    return;
  atomic_fetch_add(&m->latency_sum_us, latency_us);
  atomic_fetch_add(&m->latency_count, 1);
  for (int i = 0; i < HIST_BUCKETS; i++) {
    if (latency_us <= hist_bounds[i]) {
      atomic_fetch_add(&m->latency_hist[i], 1);
      break;
    }
  }
}

void metrics_handle(np_metrics_t *m, conn_t *conn, http_request_t *req) {
  char body[4096];
  int n = 0;

  n += snprintf(body + n, sizeof(body) - (usize)n,
                "# HELP nproxy_requests_total Total HTTP requests\n"
                "# TYPE nproxy_requests_total counter\n"
                "nproxy_requests_total %llu\n"
                "nproxy_requests_2xx_total %llu\n"
                "nproxy_requests_4xx_total %llu\n"
                "nproxy_requests_5xx_total %llu\n"
                "# HELP nproxy_active_connections Active connections\n"
                "# TYPE nproxy_active_connections gauge\n"
                "nproxy_active_connections %llu\n"
                "# HELP nproxy_upstream_errors_total Upstream errors\n"
                "# TYPE nproxy_upstream_errors_total counter\n"
                "nproxy_upstream_errors_total %llu\n",
                (unsigned long long)atomic_load(&m->requests_total),
                (unsigned long long)atomic_load(&m->requests_2xx),
                (unsigned long long)atomic_load(&m->requests_4xx),
                (unsigned long long)atomic_load(&m->requests_5xx),
                (unsigned long long)atomic_load(&m->active_connections),
                (unsigned long long)atomic_load(&m->upstream_errors));

  n += snprintf(
      body + n, sizeof(body) - (usize)n,
      "# HELP nproxy_request_duration_seconds Request duration histogram\n"
      "# TYPE nproxy_request_duration_seconds histogram\n");

  static const char *bucket_labels[] = {
      "0.0001", "0.0005", "0.001", "0.002", "0.005", "0.01", "0.02", "0.05",
      "0.1",    "0.2",    "0.5",   "1.0",   "2.0",   "5.0",  "10.0", "+Inf"};
  for (int i = 0; i < HIST_BUCKETS; i++) {
    n += snprintf(body + n, sizeof(body) - (usize)n,
                  "nproxy_request_duration_seconds_bucket{le=\"%s\"} %llu\n",
                  bucket_labels[i],
                  (unsigned long long)atomic_load(&m->latency_hist[i]));
  }

  u64 count = atomic_load(&m->latency_count);
  u64 sum = atomic_load(&m->latency_sum_us);
  n += snprintf(body + n, sizeof(body) - (usize)n,
                "nproxy_request_duration_seconds_count %llu\n"
                "nproxy_request_duration_seconds_sum %f\n",
                (unsigned long long)count, (double)sum / 1e6);

  NP_UNUSED(n);

  response_write_simple(&conn->wbuf, 200, "OK", "text/plain; version=0.0.4",
                        body, req->keep_alive);
  conn->state = CONN_WRITING_RESPONSE;
}
