#ifndef NPROXY_MODULE_H
#define NPROXY_MODULE_H

#include "core/config.h"
#include "http/request.h"
#include "net/conn.h"

// Return codes for module interceptors
#define NP_MODULE_DECLINED 0  // Let normal Nproxy handle it
#define NP_MODULE_HANDLED 1   // Module sent response, don't continue
#define NP_MODULE_ERROR -1    // Internal module error

typedef struct nproxy_module_s {
  const char *name;
  const char *version;
  int (*init)(np_config_t *cfg);
  int (*request_handler)(conn_t *conn, http_request_t *req, np_config_t *cfg);
  void (*destroy)(void);
} nproxy_module_t;

// Loaded Module registry
typedef struct {
  void *handle;
  nproxy_module_t *module;
} loaded_module_t;

int module_load_all(np_config_t *cfg);
int module_run_request_handlers(conn_t *conn, http_request_t *req, np_config_t *cfg);
void module_unload_all(void);

#endif
