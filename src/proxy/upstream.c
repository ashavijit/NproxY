#include "proxy/upstream.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "net/socket.h"
#include "proxy/balancer.h"

upstream_pool_t *upstream_pool_create(const np_server_config_t *cfg) {
  upstream_pool_t *pool = malloc(sizeof(*pool));
  if (!pool)
    return NULL;
  memset(pool, 0, sizeof(*pool));
  pool->mode = cfg->proxy.mode;
  pool->rr_index = 0;
  pool->count = cfg->proxy.backend_count;
  pool->keepalive_conns = cfg->proxy.keepalive_conns;

  for (int i = 0; i < pool->count; i++) {
    strncpy(pool->backends[i].host, cfg->proxy.backends[i].host,
            sizeof(pool->backends[i].host) - 1);
    pool->backends[i].port = cfg->proxy.backends[i].port;
    pool->backends[i].healthy = true;
    pool->backends[i].active_conns = 0;
    pool->backends[i].error_count = 0;
    pool->backends[i].idle_count = 0;
  }
  log_info("upstream pool: %d backends, mode=%s", pool->count,
           pool->mode == BALANCE_LEAST_CONN ? "least_conn" : "round_robin");
  return pool;
}

void upstream_pool_destroy(upstream_pool_t *pool) {
  free(pool);
}

upstream_backend_t *upstream_select(upstream_pool_t *pool) {
  if (pool->count == 0)
    return NULL;
  upstream_backend_t *be;
  if (pool->mode == BALANCE_LEAST_CONN) {
    be = balancer_least_conn(pool);
  } else {
    be = balancer_round_robin(pool);
  }
  if (be)
    be->active_conns++;
  return be;
}

void upstream_release(upstream_pool_t *pool, upstream_backend_t *be, bool error) {
  NP_UNUSED(pool);
  if (!be)
    return;
  if (be->active_conns > 0)
    be->active_conns--;
  if (error) {
    be->error_count++;
    if (be->error_count > 5) {
      be->healthy = false;
      log_warn("upstream %s:%d marked unhealthy (errors=%d)", be->host, be->port, be->error_count);
    }
  } else {
    be->total_requests++;
    if (!be->healthy && be->error_count == 0)
      be->healthy = true;
  }
}

int upstream_get_connection(upstream_pool_t *pool, upstream_backend_t *be) {
  NP_UNUSED(pool);
  if (be->idle_count > 0) {
    be->idle_count--;
    int fd = be->idle_fds[be->idle_count];
    return fd;
  }

  int ufd;
  if (socket_connect_nonblock(&ufd, be->host, be->port) != NP_OK) {
    return -1;
  }
  return ufd;
}

void upstream_put_connection(upstream_pool_t *pool, upstream_backend_t *be, int fd) {
  if (!be) {
    close(fd);
    return;
  }

  if (be->idle_count < pool->keepalive_conns && be->idle_count < 64) {
    be->idle_fds[be->idle_count] = fd;
    be->idle_count++;
  } else {
    close(fd);
  }
}
