#ifndef NPROXY_HTTP_PARSER_H
#define NPROXY_HTTP_PARSER_H

#include "core/string_util.h"
#include "core/types.h"

typedef enum {
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_POST = 1,
  HTTP_METHOD_PUT = 2,
  HTTP_METHOD_DELETE = 3,
  HTTP_METHOD_HEAD = 4,
  HTTP_METHOD_OPTIONS = 5,
  HTTP_METHOD_PATCH = 6,
  HTTP_METHOD_UNKNOWN = 7,
} http_method_t;

typedef enum {
  HTTP_10 = 10,
  HTTP_11 = 11,
} http_version_t;

typedef struct {
  str_t name;
  str_t value;
} http_header_t;

typedef enum {
  PARSE_INCOMPLETE = 0,
  PARSE_DONE = 1,
  PARSE_ERROR = -1,
} parse_result_t;

typedef struct {
  http_method_t method;
  str_t uri;
  http_version_t version;
  http_header_t headers[NP_MAX_HEADERS];
  int header_count;
  i64 content_length;
  bool chunked;
  bool keep_alive;
  bool has_connection_header; /* true if Connection: header was seen */
  usize body_offset;
  usize parsed_bytes;
} http_parse_state_t;

void http_parse_state_init(http_parse_state_t *s);
parse_result_t http_parse_request(http_parse_state_t *s, const u8 *data, usize len);

const char *http_method_str(http_method_t m);

#endif
