#include "proxy/balancer.h"

#include <limits.h>

upstream_backend_t *balancer_round_robin(upstream_pool_t *pool) {
  int start = pool->rr_index;
  int count = pool->count;
  for (int i = 0; i < count; i++) {
    int idx = (start + i) % count;
    if (pool->backends[idx].healthy) {
      pool->rr_index = (idx + 1) % count;
      return &pool->backends[idx];
    }
  }
  return NULL;
}

upstream_backend_t *balancer_least_conn(upstream_pool_t *pool) {
  upstream_backend_t *best = NULL;
  int min_conns = INT_MAX;
  for (int i = 0; i < pool->count; i++) {
    upstream_backend_t *be = &pool->backends[i];
    if (!be->healthy) continue;
    if (be->active_conns < min_conns) {
      min_conns = be->active_conns;
      best = be;
    }
  }
  return best;
}
