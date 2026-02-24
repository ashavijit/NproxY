#include <stdio.h>
#include <string.h>

#include "http/response.h"
#include "module/module.h"

// Initialize the plugin
static int hello_init(np_config_t *cfg) {
  printf("[Hello Module] Initialized ok! Worker count: %d\n", cfg->worker_processes);
  return 0;
}

// Intercept specific requests
static int hello_handler(conn_t *conn, http_request_t *req, np_config_t *cfg) {
  (void)cfg;

  if (req->path.len == 6 && strncmp(req->path.ptr, "/hello", 6) == 0) {
    const char *body = "<h1>Hello from Dynamic Nproxy Module!</h1>";
    response_write_simple(&conn->wbuf, 200, "OK", "text/html", body, req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    return NP_MODULE_HANDLED;
  }

  return NP_MODULE_DECLINED;
}

// Cleanup the plugin
static void hello_destroy(void) {
  printf("[Hello Module] Destroyed gracefully.\n");
}

// Export the module structure
nproxy_module_t nproxy_module = {.name = "hello_module",
                                 .version = "1.0.0",
                                 .init = hello_init,
                                 .request_handler = hello_handler,
                                 .destroy = hello_destroy};
