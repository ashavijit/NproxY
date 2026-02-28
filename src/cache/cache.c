#include "cache/cache.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core/log.h"

static u64 fnv1a(const char *data, usize len) {
  u64 hash = 14695981039346656037ULL;
  for (usize i = 0; i < len; i++) {
    hash ^= (u8)data[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

static void cache_path(cache_store_t *store, const char *key, char *out, usize max) {
  u64 h = fnv1a(key, strlen(key));
  snprintf(out, max, "%s/%016llx.cache", store->root, (unsigned long long)h);
}

cache_store_t *cache_store_create(const char *root, int max_entries) {
  mkdir(root, 0755);

  cache_store_t *store = calloc(1, sizeof(cache_store_t));
  if (!store)
    return NULL;

  strncpy(store->root, root, sizeof(store->root) - 1);
  store->max_entries = max_entries;
  return store;
}

void cache_store_destroy(cache_store_t *store) {
  if (store)
    free(store);
}

np_status_t cache_lookup(cache_store_t *store, const char *key, cache_entry_t *entry) {
  char path[1024];
  cache_path(store, key, path, sizeof(path));

  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return NP_ERR;

  cache_entry_header_t hdr;
  if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    close(fd);
    return NP_ERR;
  }

  if (hdr.expire_time > 0 && time(NULL) > hdr.expire_time) {
    close(fd);
    unlink(path);
    return NP_ERR;
  }

  usize total = hdr.headers_len + hdr.body_len;
  u8 *data = malloc(total);
  if (!data) {
    close(fd);
    return NP_ERR_NOMEM;
  }

  isize nr = read(fd, data, total);
  close(fd);

  if (nr < 0 || (usize)nr != total) {
    free(data);
    return NP_ERR;
  }

  entry->hdr = hdr;
  entry->data = data;
  entry->data_len = total;
  return NP_OK;
}

np_status_t cache_insert(cache_store_t *store, const char *key, int status, const u8 *headers,
                         usize headers_len, const u8 *body, usize body_len, int ttl) {
  char path[1024];
  char tmp_path[1040];
  cache_path(store, key, path, sizeof(path));
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

  int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    return NP_ERR;

  cache_entry_header_t hdr;
  hdr.status = status;
  hdr.headers_len = headers_len;
  hdr.body_len = body_len;
  hdr.expire_time = (ttl > 0) ? time(NULL) + ttl : 0;

  if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    close(fd);
    unlink(tmp_path);
    return NP_ERR;
  }

  if (headers_len > 0 && write(fd, headers, headers_len) != (isize)headers_len) {
    close(fd);
    unlink(tmp_path);
    return NP_ERR;
  }

  if (body_len > 0 && write(fd, body, body_len) != (isize)body_len) {
    close(fd);
    unlink(tmp_path);
    return NP_ERR;
  }

  close(fd);

  if (rename(tmp_path, path) != 0) {
    unlink(tmp_path);
    return NP_ERR;
  }

  return NP_OK;
}

np_status_t cache_remove(cache_store_t *store, const char *key) {
  char path[1024];
  cache_path(store, key, path, sizeof(path));
  if (unlink(path) == 0)
    return NP_OK;
  return NP_ERR;
}

void cache_key_from_request(http_request_t *req, char *out, usize max) {
  str_t host = request_header(req, STR("Host"));
  const char *method = http_method_str(req->method);

  if (host.ptr) {
    snprintf(out, max, "%s:" STR_FMT ":" STR_FMT, method, STR_ARG(host), STR_ARG(req->path));
  } else {
    snprintf(out, max, "%s:_:" STR_FMT, method, STR_ARG(req->path));
  }
}
