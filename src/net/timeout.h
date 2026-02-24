#ifndef NPROXY_TIMEOUT_H
#define NPROXY_TIMEOUT_H

#include "core/types.h"
#include <time.h>

typedef void (*timeout_cb_t)(void *ctx);

typedef struct timeout_entry timeout_entry_t;
typedef struct timeout_wheel timeout_wheel_t;

struct timeout_entry {
  timeout_cb_t cb;
  void *ctx;
  time_t deadline;
  timeout_entry_t *next;
  timeout_entry_t *prev;
  int bucket;
  bool active;
};

timeout_wheel_t *timeout_wheel_create(int nbuckets, int resolution_sec);
void timeout_wheel_destroy(timeout_wheel_t *tw);

timeout_entry_t *timeout_add(timeout_wheel_t *tw, int seconds, timeout_cb_t cb,
                             void *ctx);
void timeout_remove(timeout_wheel_t *tw, timeout_entry_t *entry);
void timeout_tick(timeout_wheel_t *tw);

#endif
