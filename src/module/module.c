#include "module.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "core/log.h"

static loaded_module_t g_modules[CONFIG_MAX_MODULES];
static int g_modules_count = 0;

int module_load_all(np_config_t *cfg) {
  g_modules_count = 0;

  for (int s = 0; s < cfg->server_count; s++) {
    for (int i = 0; i < cfg->servers[s].modules.count; i++) {
      const char *path = cfg->servers[s].modules.paths[i];
      void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
      if (!handle) {
        log_error("module: failed to load %s: %s", path, dlerror());
        return NP_MODULE_ERROR;
      }

      nproxy_module_t *mod = (nproxy_module_t *)dlsym(handle, "nproxy_module");
      if (!mod) {
        log_error("module: %s does not export 'nproxy_module'", path);
        dlclose(handle);
        return NP_MODULE_ERROR;
      }

      if (mod->init) {
        if (mod->init(cfg) != 0) {
          log_error("module: '%s' failed initialization", mod->name);
          dlclose(handle);
          return NP_MODULE_ERROR;
        }
      }

      g_modules[g_modules_count].handle = handle;
      g_modules[g_modules_count].module = mod;
      g_modules_count++;

      log_info("module: successfully loaded '%s' (v%s)", mod->name, mod->version);
    }
  }

  return 0;
}

int module_run_request_handlers(conn_t *conn, http_request_t *req, np_config_t *cfg) {
  for (int i = 0; i < g_modules_count; i++) {
    nproxy_module_t *mod = g_modules[i].module;
    if (mod->request_handler) {
      int rc = mod->request_handler(conn, req, cfg);
      if (rc == NP_MODULE_HANDLED) {
        return NP_MODULE_HANDLED;
      }
    }
  }
  return NP_MODULE_DECLINED;
}

void module_unload_all(void) {
  for (int i = 0; i < g_modules_count; i++) {
    nproxy_module_t *mod = g_modules[i].module;
    if (mod->destroy) {
      mod->destroy();
    }
    dlclose(g_modules[i].handle);
  }
  g_modules_count = 0;
}
