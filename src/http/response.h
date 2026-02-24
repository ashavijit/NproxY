#ifndef NPROXY_RESPONSE_H
#define NPROXY_RESPONSE_H

#include "core/memory.h"
#include "core/string_util.h"
#include "core/types.h"
#include "http/parser.h"
#include "net/buffer.h"

typedef struct {
  int status;
  const char *reason;
  http_header_t headers[NP_MAX_HEADERS];
  int header_count;
  str_t body;
  bool keep_alive;
  bool chunked;
} http_response_t;

http_response_t *response_create(arena_t *arena);
void response_set_header(http_response_t *r, arena_t *arena, const char *name, const char *value);
np_status_t response_serialize(http_response_t *r, np_buf_t *buf);

void response_write_simple(np_buf_t *buf, int status, const char *reason, const char *content_type,
                           const char *body, bool keep_alive);

#endif
