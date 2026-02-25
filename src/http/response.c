#include "http/response.h"

#include <stdio.h>
#include <string.h>

#include "core/log.h"

http_response_t *response_create(arena_t *arena) {
  http_response_t *r = arena_new(arena, http_response_t);
  if (!r)
    return NULL;
  memset(r, 0, sizeof(*r));
  r->status = 200;
  r->reason = "OK";
  return r;
}

void response_set_header(http_response_t *r, arena_t *arena, const char *name, const char *value) {
  if (r->header_count >= NP_MAX_HEADERS)
    return;
  usize nlen = strlen(name);
  usize vlen = strlen(value);
  char *n = arena_alloc(arena, nlen + 1);
  char *v = arena_alloc(arena, vlen + 1);
  if (!n || !v)
    return;
  memcpy(n, name, nlen);
  n[nlen] = '\0';
  memcpy(v, value, vlen);
  v[vlen] = '\0';
  r->headers[r->header_count].name = (str_t){.ptr = n, .len = nlen};
  r->headers[r->header_count].value = (str_t){.ptr = v, .len = vlen};
  r->header_count++;
}

static const char *status_reason(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 204:
      return "No Content";
    case 206:
      return "Partial Content";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 304:
      return "Not Modified";
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 413:
      return "Payload Too Large";
    case 429:
      return "Too Many Requests";
    case 500:
      return "Internal Server Error";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return "Unknown";
  }
}

np_status_t response_serialize(http_response_t *r, np_buf_t *buf) {
  const char *reason = r->reason ? r->reason : status_reason(r->status);
  char tmp[256];
  int n = snprintf(tmp, sizeof(tmp), "HTTP/1.1 %d %s\r\n", r->status, reason);
  if (buf_writable(buf) < (usize)n)
    buf_compact(buf);
  memcpy(buf_write_ptr(buf), tmp, (usize)n);
  buf_produce(buf, (usize)n);

  for (int i = 0; i < r->header_count; i++) {
    usize hlen = r->headers[i].name.len + r->headers[i].value.len + 4;
    if (buf_writable(buf) < hlen)
      buf_compact(buf);
    u8 *p = buf_write_ptr(buf);
    memcpy(p, r->headers[i].name.ptr, r->headers[i].name.len);
    p += r->headers[i].name.len;
    *p++ = ':';
    *p++ = ' ';
    memcpy(p, r->headers[i].value.ptr, r->headers[i].value.len);
    p += r->headers[i].value.len;
    *p++ = '\r';
    *p++ = '\n';
    buf_produce(buf, hlen);
  }

  const char *conn_hdr =
      r->keep_alive ? "Connection: keep-alive\r\n\r\n" : "Connection: close\r\n\r\n";
  usize clen = strlen(conn_hdr);
  if (buf_writable(buf) < clen)
    buf_compact(buf);
  memcpy(buf_write_ptr(buf), conn_hdr, clen);
  buf_produce(buf, clen);

  if (r->body.len > 0) {
    char cl[64];
    int cn = snprintf(cl, sizeof(cl), "Content-Length: %zu\r\n", r->body.len);
    NP_UNUSED(cn);

    if (buf_writable(buf) < r->body.len)
      buf_compact(buf);
    if (buf_writable(buf) >= r->body.len) {
      memcpy(buf_write_ptr(buf), r->body.ptr, r->body.len);
      buf_produce(buf, r->body.len);
    }
  }
  return NP_OK;
}

void response_write_simple(np_buf_t *buf, int status, const char *reason, const char *content_type,
                           const char *body, bool keep_alive) {
  usize body_len = body ? strlen(body) : 0;
  const char *conn = keep_alive ? "keep-alive" : "close";
  char header[512];
  int hn = snprintf(header, sizeof(header),
                    "HTTP/1.1 %d %s\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: %s\r\n"
                    "\r\n",
                    status, reason ? reason : status_reason(status),
                    content_type ? content_type : "text/plain", body_len, conn);
  if (hn > 0 && buf_writable(buf) >= (usize)hn) {
    memcpy(buf_write_ptr(buf), header, (usize)hn);
    buf_produce(buf, (usize)hn);
  }
  if (body_len > 0 && buf_writable(buf) >= body_len) {
    memcpy(buf_write_ptr(buf), body, body_len);
    buf_produce(buf, body_len);
  }
}

void response_write_error(np_buf_t *buf, int status, bool keep_alive) {
  const char *reason = status_reason(status);
  char body[2048];
  int blen = snprintf(
      body, sizeof(body),
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head><title>%d %s</title></head>\n"
      "<body style=\"font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
      "display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;"
      "background:#0f0f0f;color:#e0e0e0\">\n"
      "<div style=\"text-align:center;padding:40px\">\n"
      "<h1 style=\"font-size:72px;font-weight:200;margin:0;color:#ff6b6b\">%d</h1>\n"
      "<p style=\"font-size:20px;color:#888;margin:12px 0 32px\">%s</p>\n"
      "<hr style=\"border:none;border-top:1px solid #333;width:280px;margin:0 auto 16px\">\n"
      "<p style=\"font-size:13px;color:#555\">nproxy/1.0</p>\n"
      "</div>\n"
      "</body>\n"
      "</html>\n",
      status, reason, status, reason);

  const char *conn = keep_alive ? "keep-alive" : "close";
  char header[512];
  int hn = snprintf(header, sizeof(header),
                    "HTTP/1.1 %d %s\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %d\r\n"
                    "Connection: %s\r\n"
                    "\r\n",
                    status, reason, blen, conn);

  if (hn > 0 && buf_writable(buf) >= (usize)hn) {
    memcpy(buf_write_ptr(buf), header, (usize)hn);
    buf_produce(buf, (usize)hn);
  }
  if (blen > 0 && buf_writable(buf) >= (usize)blen) {
    memcpy(buf_write_ptr(buf), body, (usize)blen);
    buf_produce(buf, (usize)blen);
  }
}
