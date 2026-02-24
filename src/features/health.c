#include "features/health.h"

#include <string.h>

#include "http/response.h"

static const char HEALTHZ_RESPONSE_KA[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 16\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

static const char HEALTHZ_RESPONSE_CLOSE[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 16\r\n"
    "Connection: close\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";

void health_handle(conn_t *conn, http_request_t *req) {
  const char *resp;
  usize len;
  if (req->keep_alive) {
    resp = HEALTHZ_RESPONSE_KA;
    len = sizeof(HEALTHZ_RESPONSE_KA) - 1;
  } else {
    resp = HEALTHZ_RESPONSE_CLOSE;
    len = sizeof(HEALTHZ_RESPONSE_CLOSE) - 1;
  }

  if (buf_writable(&conn->wbuf) >= len) {
    memcpy(buf_write_ptr(&conn->wbuf), resp, len);
    buf_produce(&conn->wbuf, len);
  }
  conn->state = CONN_WRITING_RESPONSE;
}
