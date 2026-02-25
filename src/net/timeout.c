#include "net/timeout.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/log.h"

struct timeout_wheel {
  timeout_entry_t **buckets;
  int nbuckets;
  int resolution;
  int current;
};

timeout_wheel_t *timeout_wheel_create(int nbuckets, int resolution_sec) {
  timeout_wheel_t *tw = malloc(sizeof(*tw));
  if (!tw)
    return NULL;
  tw->buckets = calloc((usize)nbuckets, sizeof(timeout_entry_t *));
  tw->nbuckets = nbuckets;
  tw->resolution = resolution_sec;
  tw->current = 0;
  if (!tw->buckets) {
    free(tw);
    return NULL;
  }
  return tw;
}

void timeout_wheel_destroy(timeout_wheel_t *tw) {
  for (int i = 0; i < tw->nbuckets; i++) {
    timeout_entry_t *e = tw->buckets[i];
    while (e) {
      timeout_entry_t *next = e->next;
      free(e);
      e = next;
    }
  }
  free(tw->buckets);
  free(tw);
}

timeout_entry_t *timeout_add(timeout_wheel_t *tw, int seconds, timeout_cb_t cb, void *ctx) {
  timeout_entry_t *e = malloc(sizeof(*e));
  if (!e)
    return NULL;
  e->cb = cb;
  e->ctx = ctx;
  e->deadline = time(NULL) + seconds;
  e->active = true;

  int slot = (tw->current + seconds / tw->resolution) % tw->nbuckets;
  e->bucket = slot;
  e->next = tw->buckets[slot];
  e->prev = NULL;
  if (tw->buckets[slot])
    tw->buckets[slot]->prev = e;
  tw->buckets[slot] = e;
  return e;
}

void timeout_remove(timeout_wheel_t *tw, timeout_entry_t *entry) {
  if (!entry || !entry->active)
    return;
  entry->active = false;
  int slot = entry->bucket;
  if (entry->prev)
    entry->prev->next = entry->next;
  else
    tw->buckets[slot] = entry->next;
  if (entry->next)
    entry->next->prev = entry->prev;
  entry->next = NULL;
  entry->prev = NULL;
  free(entry);
}

void timeout_tick(timeout_wheel_t *tw) {
  time_t now = time(NULL);
  tw->current = (tw->current + 1) % tw->nbuckets;
  timeout_entry_t *e = tw->buckets[tw->current];
  tw->buckets[tw->current] = NULL;

  while (e) {
    timeout_entry_t *next = e->next;
    if (e->active && e->deadline <= now) {
      e->active = false;
      e->cb(e->ctx);
      free(e);
    } else if (e->active) {
      int slot = (tw->current + 1) % tw->nbuckets;
      e->bucket = slot;
      e->next = tw->buckets[slot];
      e->prev = NULL;
      if (tw->buckets[slot])
        tw->buckets[slot]->prev = e;
      tw->buckets[slot] = e;
    } else {
      free(e);
    }
    e = next;
  }
}
