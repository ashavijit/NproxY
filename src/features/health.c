#include "features/health.h"
#include "http/response.h"

static const char HEALTH_BODY[] = "{\"status\":\"ok\"}\n";

void health_handle(conn_t *conn, http_request_t *req) {
  response_write_simple(&conn->wbuf, 200, "OK", "application/json", HEALTH_BODY,
                        req->keep_alive);
  conn->state = CONN_WRITING_RESPONSE;
}
