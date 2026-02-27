#ifndef NPROXY_CACHE_H
#define NPROXY_CACHE_H

#include "core/types.h"
#include "http/request.h"

typedef struct {
  int status;
  usize headers_len;
  usize body_len;
  time_t expire_time;
} cache_entry_header_t;

typedef struct {
  cache_entry_header_t hdr;
  u8 *data;
  usize data_len;
} cache_entry_t;

typedef struct {
  char root[512];
  int max_entries;
} cache_store_t;

cache_store_t *cache_store_create(const char *root, int max_entries);
void cache_store_destroy(cache_store_t *store);

np_status_t cache_lookup(cache_store_t *store, const char *key, cache_entry_t *entry);
np_status_t cache_insert(cache_store_t *store, const char *key, int status, const u8 *headers,
                         usize headers_len, const u8 *body, usize body_len, int ttl);
np_status_t cache_remove(cache_store_t *store, const char *key);

void cache_key_from_request(http_request_t *req, char *out, usize max);

#endif
