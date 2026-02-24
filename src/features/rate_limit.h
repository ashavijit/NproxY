#ifndef NPROXY_RATE_LIMIT_H
#define NPROXY_RATE_LIMIT_H

#include "core/config.h"
#include "core/types.h"

typedef struct rate_limiter rate_limiter_t;

rate_limiter_t *rate_limiter_create(const np_config_t *cfg);
void rate_limiter_destroy(rate_limiter_t *rl);
np_status_t rate_limit_check(rate_limiter_t *rl, const char *ip);

#endif
