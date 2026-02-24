#ifndef NPROXY_UPSTREAM_H
#define NPROXY_UPSTREAM_H

#include "core/config.h"
#include "core/types.h"

typedef struct upstream_backend upstream_backend_t;
typedef struct upstream_pool upstream_pool_t;

struct upstream_backend {
  char host[256];
  u16 port;
  int active_conns;
  int total_requests;
  int error_count;
  bool healthy;
};

struct upstream_pool {
  upstream_backend_t backends[CONFIG_MAX_BACKENDS];
  int count;
  int rr_index;
  balance_mode_t mode;
};

upstream_pool_t *upstream_pool_create(const np_config_t *cfg);
void upstream_pool_destroy(upstream_pool_t *pool);
upstream_backend_t *upstream_select(upstream_pool_t *pool);
void upstream_release(upstream_pool_t *pool, upstream_backend_t *be,
                      bool error);

#endif
