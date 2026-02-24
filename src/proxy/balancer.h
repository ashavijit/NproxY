#ifndef NPROXY_BALANCER_H
#define NPROXY_BALANCER_H

#include "proxy/upstream.h"

typedef upstream_backend_t *(*balancer_fn)(upstream_pool_t *pool);

upstream_backend_t *balancer_round_robin(upstream_pool_t *pool);
upstream_backend_t *balancer_least_conn(upstream_pool_t *pool);

#endif
