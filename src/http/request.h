#ifndef NPROXY_REQUEST_H
#define NPROXY_REQUEST_H

#include <netinet/in.h>

#include "core/memory.h"
#include "core/string_util.h"
#include "core/types.h"
#include "http/parser.h"

typedef struct {
  http_method_t method;
  str_t path;
  str_t query;
  http_version_t version;
  http_header_t headers[NP_MAX_HEADERS];
  int header_count;
  i64 content_length;
  bool chunked;
  bool keep_alive;
  str_t body;
  char remote_ip[INET_ADDRSTRLEN];
  u64 recv_ts_us;
} http_request_t;

http_request_t *request_create(arena_t *arena);
np_status_t request_populate(http_request_t *req, http_parse_state_t *ps, const u8 *raw, usize len);
str_t request_header(const http_request_t *req, str_t name);

#endif
