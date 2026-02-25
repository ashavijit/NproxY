#include "http/request.h"

#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "core/string_util.h"

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

http_request_t *request_create(arena_t *arena) {
  http_request_t *req = arena_new(arena, http_request_t);
  if (!req)
    return NULL;
  memset(req, 0, sizeof(*req));
  req->content_length = -1;
  return req;
}

np_status_t request_populate(http_request_t *req, http_parse_state_t *ps, const u8 *raw,
                             usize len) {
  req->method = ps->method;
  req->version = ps->version;
  req->content_length = ps->content_length;
  req->chunked = ps->chunked;
  req->keep_alive = ps->keep_alive;
  req->header_count = ps->header_count;
  req->upgrade = ps->upgrade;
  req->upgrade_protocol = ps->upgrade_protocol;

  str_t uri = ps->uri;
  const char *q = memchr(uri.ptr, '?', uri.len);
  if (q) {
    req->path = (str_t){.ptr = uri.ptr, .len = (usize)(q - uri.ptr)};
    req->query = (str_t){.ptr = q + 1, .len = uri.len - (usize)(q - uri.ptr) - 1};
  } else {
    req->path = uri;
    req->query = STR_NULL;
  }

  memcpy(req->headers, ps->headers, (usize)ps->header_count * sizeof(http_header_t));

  if (ps->content_length > 0 && ps->body_offset + (usize)ps->content_length <= len) {
    req->body.ptr = (const char *)(raw + ps->body_offset);
    req->body.len = (usize)ps->content_length;
  }

  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  req->recv_ts_us = (u64)ts.tv_sec * 1000000ULL + (u64)(ts.tv_nsec / 1000);

  return NP_OK;
}

str_t request_header(const http_request_t *req, str_t name) {
  for (int i = 0; i < req->header_count; i++) {
    if (str_ieq(req->headers[i].name, name)) {
      return req->headers[i].value;
    }
  }
  return STR_NULL;
}
