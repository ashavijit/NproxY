# Dynamic Modules

Nproxy supports a plugin system based on shared object (`.so`) files loaded at startup via `dlopen`. Modules can intercept requests before the normal routing chain, enabling custom handlers without modifying Nproxy's source code.

---

## Loading a Module

Add a `load_module` directive to the `[server]` block:

```ini
[server]
listen_port = 8080
load_module = ./tests/hello_module.so
```

Multiple modules can be loaded (up to 16 per server block):

```ini
[server]
load_module = ./modules/auth.so
load_module = ./modules/rewrite.so
load_module = ./modules/analytics.so
```

---

## Writing a Module

A module is a shared library that exports a single symbol: `nproxy_module` of type `nproxy_module_t`.

### Module Interface

```c
// Defined in src/module/module.h

#define NP_MODULE_DECLINED  0   // Let normal Nproxy handle it
#define NP_MODULE_HANDLED   1   // Module sent response, stop chain
#define NP_MODULE_ERROR    -1   // Internal module error

typedef struct nproxy_module_s {
    const char *name;
    const char *version;
    int  (*init)(np_config_t *cfg);
    int  (*request_handler)(conn_t *conn, http_request_t *req, np_config_t *cfg);
    void (*destroy)(void);
} nproxy_module_t;
```

| Field | Required | Description |
|---|---|---|
| `name` | Yes | Human-readable module name (for logging) |
| `version` | Yes | Module version string |
| `init` | No | Called once at startup. Return `0` on success, non-zero to abort |
| `request_handler` | No | Called for every request. Return `NP_MODULE_HANDLED` to stop routing, `NP_MODULE_DECLINED` to pass through |
| `destroy` | No | Called at shutdown for cleanup |

### Example: Hello Module

```c
// hello_module.c
#include <stdio.h>
#include <string.h>
#include "http/response.h"
#include "module/module.h"

static int hello_init(np_config_t *cfg) {
    printf("[Hello Module] Initialized! Workers: %d\n", cfg->worker_processes);
    return 0;
}

static int hello_handler(conn_t *conn, http_request_t *req, np_config_t *cfg) {
    (void)cfg;

    // Only handle GET /hello
    if (req->path.len == 6 && strncmp(req->path.ptr, "/hello", 6) == 0) {
        const char *body = "<h1>Hello from Dynamic Nproxy Module!</h1>";
        response_write_simple(&conn->wbuf, 200, "OK", "text/html", body, req->keep_alive);
        conn->state = CONN_WRITING_RESPONSE;
        return NP_MODULE_HANDLED;
    }

    return NP_MODULE_DECLINED;  // Not our path, let Nproxy handle it
}

static void hello_destroy(void) {
    printf("[Hello Module] Destroyed gracefully.\n");
}

// The exported symbol Nproxy looks for
nproxy_module_t nproxy_module = {
    .name            = "hello_module",
    .version         = "1.0.0",
    .init            = hello_init,
    .request_handler = hello_handler,
    .destroy         = hello_destroy,
};
```

---

## Compiling a Module

Modules must be compiled as position-independent shared objects and linked against Nproxy's headers:

```bash
gcc -shared -fPIC -o hello_module.so hello_module.c \
    -Isrc -std=c17 -D_GNU_SOURCE
```

The `-Isrc` flag gives the module access to Nproxy's header files (`module/module.h`, `http/response.h`, etc.).

---

## Module Execution Order

1. Modules are loaded in the order they appear in the config file
2. On each request, `module_run_request_handlers()` iterates through all loaded modules
3. The **first module** to return `NP_MODULE_HANDLED` wins -- subsequent modules and normal routing are skipped
4. If all modules return `NP_MODULE_DECLINED`, normal routing continues (rate limit -> metrics -> healthz -> proxy -> static -> 404)

---

## Module Lifecycle

```
Startup:
  config_load() → module_load_all()
                    ├── dlopen(path)
                    ├── dlsym("nproxy_module")
                    └── mod->init(cfg)

Per Request:
  handler_dispatch()
    └── module_run_request_handlers()
          └── mod->request_handler(conn, req, cfg)
                ├── NP_MODULE_HANDLED → stop, response sent
                └── NP_MODULE_DECLINED → continue to next module / normal routing

Shutdown:
  module_unload_all()
    ├── mod->destroy()
    └── dlclose(handle)
```

---

## Available APIs for Modules

Modules have access to:

| Header | Key Types/Functions |
|---|---|
| `module/module.h` | `nproxy_module_t`, return codes |
| `http/request.h` | `http_request_t`, `request_header()`, `STR()` macro |
| `http/response.h` | `response_write_simple()`, `response_write_error()` |
| `net/conn.h` | `conn_t`, `conn_state_t`, buffer operations |
| `core/config.h` | `np_config_t`, `np_server_config_t` |
| `core/types.h` | Type aliases (`u8`, `u16`, `str_t`, etc.) |

### Writing a Response

```c
// Simple text/HTML response
response_write_simple(&conn->wbuf, 200, "OK", "text/html", "<h1>Hi</h1>", req->keep_alive);
conn->state = CONN_WRITING_RESPONSE;
return NP_MODULE_HANDLED;

// Error response
response_write_error(&conn->wbuf, 403, req->keep_alive);
conn->state = CONN_WRITING_RESPONSE;
return NP_MODULE_HANDLED;
```

### Reading Request Headers

```c
str_t auth = request_header(req, STR("Authorization"));
if (auth.ptr) {
    // auth.ptr is the header value, auth.len is its length
}
```

### Checking the Request Path

```c
if (req->path.len >= 4 && strncmp(req->path.ptr, "/api", 4) == 0) {
    // handle /api/* routes
}
```

---

## Limitations

- Modules are loaded at startup and cannot be hot-reloaded (requires restart or `SIGHUP`)
- Max 16 modules per server block (`CONFIG_MAX_MODULES`)
- Modules share the process address space -- a crash in a module crashes the worker
- No sandboxing or capability restrictions on module code
