#include "http/parser.h"

#include <ctype.h>
#include <string.h>

#include "core/string_util.h"

void http_parse_state_init(http_parse_state_t *s) {
  memset(s, 0, sizeof(*s));
  s->content_length = -1;
}

static http_method_t parse_method(const char *p, usize len) {
  if (len == 3 && memcmp(p, "GET", 3) == 0) return HTTP_METHOD_GET;
  if (len == 4 && memcmp(p, "POST", 4) == 0) return HTTP_METHOD_POST;
  if (len == 3 && memcmp(p, "PUT", 3) == 0) return HTTP_METHOD_PUT;
  if (len == 6 && memcmp(p, "DELETE", 6) == 0) return HTTP_METHOD_DELETE;
  if (len == 4 && memcmp(p, "HEAD", 4) == 0) return HTTP_METHOD_HEAD;
  if (len == 7 && memcmp(p, "OPTIONS", 7) == 0) return HTTP_METHOD_OPTIONS;
  if (len == 5 && memcmp(p, "PATCH", 5) == 0) return HTTP_METHOD_PATCH;
  return HTTP_METHOD_UNKNOWN;
}

static const char *find_crlf(const char *s, usize len) {
  for (usize i = 0; i + 1 < len; i++) {
    if (s[i] == '\r' && s[i + 1] == '\n') return s + i;
  }
  return NULL;
}

parse_result_t http_parse_request(http_parse_state_t *s, const u8 *data, usize len) {
  const char *buf = (const char *)data;
  const char *end = buf + len;
  const char *cur = buf;

  const char *line_end = find_crlf(cur, (usize)(end - cur));
  if (!line_end) return PARSE_INCOMPLETE;

  const char *sp1 = memchr(cur, ' ', (usize)(line_end - cur));
  if (!sp1) return PARSE_ERROR;

  s->method = parse_method(cur, (usize)(sp1 - cur));
  cur = sp1 + 1;

  const char *sp2 = memchr(cur, ' ', (usize)(line_end - cur));
  if (!sp2) return PARSE_ERROR;
  s->uri = (str_t){.ptr = cur, .len = (usize)(sp2 - cur)};
  cur = sp2 + 1;

  usize ver_len = (usize)(line_end - cur);
  if (ver_len == 8 && memcmp(cur, "HTTP/1.1", 8) == 0)
    s->version = HTTP_11;
  else if (ver_len == 8 && memcmp(cur, "HTTP/1.0", 8) == 0)
    s->version = HTTP_10;
  else
    return PARSE_ERROR;

  cur = line_end + 2;

  while (cur < end) {
    line_end = find_crlf(cur, (usize)(end - cur));
    if (!line_end) return PARSE_INCOMPLETE;

    if (line_end == cur) {
      cur += 2;
      break;
    }

    const char *colon = memchr(cur, ':', (usize)(line_end - cur));
    if (!colon) return PARSE_ERROR;

    if (s->header_count >= NP_MAX_HEADERS) return PARSE_ERROR;

    str_t name = str_trim((str_t){.ptr = cur, .len = (usize)(colon - cur)});
    str_t value = str_trim((str_t){.ptr = colon + 1, .len = (usize)(line_end - colon - 1)});

    s->headers[s->header_count].name = name;
    s->headers[s->header_count].value = value;
    s->header_count++;

    if (str_ieq(name, STR("Content-Length"))) {
      i64 cl;
      if (str_to_int(value, &cl) == 0) s->content_length = cl;
    } else if (str_ieq(name, STR("Transfer-Encoding"))) {
      if (str_ieq(value, STR("chunked"))) s->chunked = true;
    } else if (str_ieq(name, STR("Connection"))) {
      /* Explicit Connection header overrides the HTTP-version default */
      s->has_connection_header = true;
      s->keep_alive = !str_ieq(value, STR("close"));
    }

    cur = line_end + 2;
  }

  /* RFC 7230 §6.3: HTTP/1.1 connections are persistent by default.
   * Only applies when no Connection header was present. */
  if (!s->has_connection_header) s->keep_alive = (s->version == HTTP_11);

  if (s->version == HTTP_11 && !s->chunked && s->content_length < 0) {
    s->content_length = 0;
  }

  s->body_offset = (usize)(cur - buf);
  s->parsed_bytes = s->body_offset;

  if (s->content_length > 0) {
    usize body_avail = len - s->body_offset;
    if (body_avail < (usize)s->content_length) return PARSE_INCOMPLETE;
    s->parsed_bytes += (usize)s->content_length;
  }

  return PARSE_DONE;
}

const char *http_method_str(http_method_t m) {
  static const char *names[] = {"GET",  "POST",    "PUT",   "DELETE",
                                "HEAD", "OPTIONS", "PATCH", "UNKNOWN"};
  if ((usize)m >= sizeof(names) / sizeof(names[0])) return "UNKNOWN";
  return names[m];
}
