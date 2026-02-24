#include "features/rate_limit.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RL_TABLE_SIZE 4096

typedef struct {
  char ip[16];
  double tokens;
  time_t last_refill;
  bool used;
} rl_bucket_t;

struct rate_limiter {
  rl_bucket_t table[RL_TABLE_SIZE];
  double rate;
  double burst;
};

static u32 ip_hash(const char *ip) {
  u32 h = 2166136261u;
  for (const char *p = ip; *p; p++) {
    h ^= (u8)*p;
    h *= 16777619u;
  }
  return h;
}

rate_limiter_t *rate_limiter_create(const np_config_t *cfg) {
  rate_limiter_t *rl = calloc(1, sizeof(*rl));
  if (!rl) return NULL;
  rl->rate = (double)cfg->rate_limit.requests_per_second;
  rl->burst = (double)cfg->rate_limit.burst;
  return rl;
}

void rate_limiter_destroy(rate_limiter_t *rl) {
  free(rl);
}

np_status_t rate_limit_check(rate_limiter_t *rl, const char *ip) {
  u32 idx = ip_hash(ip) % RL_TABLE_SIZE;
  rl_bucket_t *b = &rl->table[idx];
  time_t now = time(NULL);

  if (!b->used || strcmp(b->ip, ip) != 0) {
    strncpy(b->ip, ip, sizeof(b->ip) - 1);
    b->tokens = rl->burst;
    b->last_refill = now;
    b->used = true;
  }

  double elapsed = (double)(now - b->last_refill);
  b->tokens += elapsed * rl->rate;
  if (b->tokens > rl->burst) b->tokens = rl->burst;
  b->last_refill = now;

  if (b->tokens < 1.0) return NP_ERR;
  b->tokens -= 1.0;
  return NP_OK;
}
