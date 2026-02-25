# Architecture

Nproxy follows the proven master/worker architecture (similar to Nginx) combined with a non-blocking, edge-triggered `epoll` event loop. This document explains the internal design in detail.

---

## High-Level Overview

```
                          ┌──────────────────────┐
                          │    master process     │
                          │  (PID file, signals)  │
                          └──────┬───────────────┘
                                 │ fork()
                ┌────────────────┼────────────────┐
                ▼                ▼                 ▼
         ┌────────────┐  ┌────────────┐   ┌────────────┐
         │  worker[0]  │  │  worker[1]  │   │  worker[N]  │
         │  epoll loop │  │  epoll loop │   │  epoll loop │
         └────────────┘  └────────────┘   └────────────┘
                │                │                 │
         accept + handle  accept + handle   accept + handle
```

---

## Master Process

**Source:** `src/proc/master.c`

The master process is responsible for:

1. **Parsing configuration** and validating it (`config_load`)
2. **Creating the listen socket(s)** with `SO_REUSEPORT` -- shared across all workers
3. **Loading dynamic modules** via `dlopen` (`module_load_all`)
4. **Forking N worker processes** (one per configured `worker_processes`)
5. **Monitoring workers** -- if a worker crashes, the master respawns it automatically
6. **Signal handling:**
   - `SIGHUP` -- graceful reload: kills all workers with `SIGTERM`, waits for them to drain, spawns new workers
   - `SIGTERM` -- graceful shutdown: signals all workers, waits, then exits
   - `SIGINT` -- immediate shutdown

The master process itself does **no request handling**. It is a pure supervisor.

---

## Worker Process

**Source:** `src/proc/worker.c`

Each worker is an independent process with its own:

| Component | Purpose |
|---|---|
| `event_loop_t` | epoll-based event loop (edge-triggered) |
| `timeout_wheel_t` | Hashed timing wheel for connection timeouts |
| `conn_pool_t` | Pre-allocated connection object pool |
| `upstream_pool_t` | Per-server upstream backend pool (one per `[server]` block) |
| `rate_limiter_t` | Token-bucket rate limiter (shared across all connections in the worker) |
| `np_metrics_t` | Atomic metrics counters (per-worker) |

### Worker Lifecycle

```
worker_run()
  │
  ├── Create event loop, timeout wheel, connection pool
  ├── Create upstream pools (one per server block with proxy enabled)
  ├── Create rate limiter and metrics (if enabled)
  ├── Initialize access logging
  ├── Register signal handlers
  ├── Register listen sockets in epoll
  │
  └── event_loop_run()   ← blocks here until shutdown
        │
        ├── on_accept_event()  → accept new connections
        │     └── conn_pool_get() → register in epoll → timeout
        │
        ├── on_client_event()  → handle reads/writes
        │     ├── handle_read()   → parse HTTP → dispatch
        │     └── handle_write()  → flush response / sendfile / proxy
        │
        └── proxy_on_upstream_event() → upstream I/O
```

---

## Event Loop

**Source:** `src/net/event_loop.c`

The event loop wraps Linux `epoll` with **edge-triggered** notification:

- **Edge-triggered** means the kernel notifies only when state *changes* (new data arrives), not continuously while data is available. This requires draining the socket fully on each event but reduces syscall overhead.
- Each file descriptor has a registered `ev_handler_t` with a callback function and context pointer.
- The loop calls `epoll_wait` with a 1-second timeout, enabling periodic checks (timeouts, signal processing).

```c
// Simplified event loop
while (*running) {
    int n = epoll_wait(epfd, events, max_events, 1000);
    for (int i = 0; i < n; i++) {
        handler->fn(fd, events, handler->ctx);
    }
}
```

### Event Types

| Constant | Meaning |
|---|---|
| `EV_READ` | Socket is readable |
| `EV_WRITE` | Socket is writable |
| `EV_HUP` | Peer hung up |
| `EV_EDGE` | Edge-triggered mode (`EPOLLET`) |

---

## Connection State Machine

**Source:** `src/net/conn.h`

Each connection (`conn_t`) transitions through these states:

```
  CONN_READING_REQUEST
        │
        ▼
  [HTTP parse complete]
        │
        ├── local handler (health/metrics) → CONN_WRITING_RESPONSE
        ├── static file → CONN_SENDFILE
        ├── reverse proxy → CONN_PROXYING
        └── WebSocket upgrade → CONN_TUNNEL
        
  CONN_WRITING_RESPONSE ──► [done] ──► keep-alive? ──► CONN_READING_REQUEST
                                              └── no ──► close
  CONN_SENDFILE ──► [sendfile complete] ──► keep-alive? ──► CONN_READING_REQUEST
  CONN_PROXYING ──► [upstream closed] ──► close
  CONN_TUNNEL ──► [bidirectional streaming until close]
```

---

## Memory Management

**Source:** `src/core/memory.c`

Nproxy uses **per-connection arena allocators** to avoid `malloc`/`free` in the hot path:

- Each connection gets a 64 KB arena (`NP_ARENA_SIZE`)
- All per-request allocations (parsed headers, URI, etc.) come from the arena
- When the request completes, `arena_reset()` reclaims all memory in O(1)
- Zero fragmentation, zero `free()` calls per request

```
Connection arena (64 KB)
┌────────────────────────────────────────────┐
│ http_request_t │ headers │ URI │ body ptr  │  ← allocated sequentially
└────────────────────────────────────────────┘
                                     ▲
                                   cursor
                                   
After arena_reset():
┌────────────────────────────────────────────┐
│                  (free)                     │
└────────────────────────────────────────────┘
▲
cursor = 0
```

---

## Request Lifecycle

A complete request flows through:

1. **Accept** (`on_accept_event`): Accept TCP connection, get a `conn_t` from the pool, register in epoll
2. **Read** (`handle_read`): Read bytes into ring buffer, parse HTTP/1.1 request
3. **Dispatch** (`handler_dispatch`):
   - Run loaded module request handlers (if any)
   - Apply URL rewrite rules (regex matching)
   - Rate limit check (token bucket)
   - Route to: `/metrics` | `/healthz` | proxy | static file | 404
4. **Respond**:
   - **Local handlers**: Write response into write buffer, flush
   - **Static files**: Write headers, then `sendfile(2)` for zero-copy body transfer
   - **Proxy**: Forward request to upstream, stream response back
5. **Keep-alive**: If `Connection: keep-alive`, reset arena and wait for next request; otherwise close

---

## Request Routing Order

The handler dispatcher (`src/http/handler.c`) checks in this order:

```
1. Dynamic modules     (module_run_request_handlers)
2. URL rewrite rules   (regex-based path rewriting)
3. Rate limiter        (429 if exceeded)
4. /metrics endpoint   (if metrics enabled)
5. /healthz endpoint   (always available)
6. Reverse proxy       (if proxy enabled for this server block)
7. Static file server  (if static_root is set)
8. 404 Not Found       (fallback)
```

The **first match wins** -- once a handler writes a response, the chain stops.

---

## Source Tree

```
src/
├── main.c                  Entry point, CLI parsing, bootstrap
│
├── core/                   Shared infrastructure
│   ├── types.h             Type aliases, error codes, compile-time limits
│   ├── config.{c,h}        INI parser, np_config_t, np_server_config_t
│   ├── log.{c,h}           Leveled logger (error/warn/info/debug)
│   ├── memory.{c,h}        Arena allocator
│   └── string_util.{c,h}   Non-owning string slices (str_t), comparisons
│
├── net/                    Networking layer
│   ├── socket.{c,h}        TCP socket creation, accept, non-blocking connect
│   ├── buffer.{c,h}        Ring buffer for reads/writes
│   ├── event_loop.{c,h}    epoll wrapper (edge-triggered)
│   ├── conn.{c,h}          Connection object and pool
│   └── timeout.{c,h}       Hashed timing wheel
│
├── http/                   HTTP/1.1 protocol
│   ├── parser.{c,h}        Zero-allocation HTTP request parser
│   ├── request.{c,h}       http_request_t construction and header access
│   ├── response.{c,h}      Response serialization helpers
│   └── handler.{c,h}       Request dispatcher / routing
│
├── proxy/                  Reverse proxy
│   ├── upstream.{c,h}      Backend pool, connection keep-alive, health tracking
│   ├── balancer.{c,h}      Round-robin and least-connections algorithms
│   └── proxy_conn.{c,h}    Proxy request forwarding, upstream event handler
│
├── static/                 Static file serving
│   ├── file_server.{c,h}   sendfile-based file serving, ETag, try_files
│   └── mime.{c,h}          File extension to MIME type mapping
│
├── proc/                   Process management
│   ├── master.{c,h}        Master process: fork, monitor, reload
│   ├── worker.{c,h}        Worker process: event loop, I/O handlers
│   └── signal.{c,h}        Signal handling (signalfd-based)
│
├── tls/                    TLS support
│   ├── tls_ctx.{c,h}       OpenSSL context setup
│   └── tls_conn.{c,h}      Per-connection TLS state
│
├── module/                 Dynamic module system
│   └── module.{c,h}        dlopen/dlsym loader, module registry
│
└── features/               Built-in features
    ├── rate_limit.{c,h}    Token-bucket rate limiter
    ├── metrics.{c,h}       Prometheus metrics endpoint
    ├── health.{c,h}        /healthz handler
    └── access_log.{c,h}    Combined-format access logging
```

---

## Key Design Decisions

| Decision | Rationale |
|---|---|
| C17, not C++ or Rust | Maximum control over memory layout, zero runtime overhead, easy to audit |
| Arena allocators | Eliminates per-request malloc/free; zero fragmentation |
| Edge-triggered epoll | Fewer syscalls than level-triggered; forces correct drain-on-read pattern |
| `sendfile(2)` for static files | Zero-copy: data goes kernel buffer -> socket, never enters userspace |
| `SO_REUSEPORT` | Each worker accepts independently; kernel distributes connections evenly |
| Fork model (not threads) | Process isolation: one worker crash doesn't affect others |
| No external dependencies (beyond OpenSSL) | Minimal attack surface, easy to build and deploy |

---

## Compile-Time Limits

Defined in `src/core/types.h`:

| Constant | Value | Meaning |
|---|---|---|
| `NP_MAX_HEADERS` | 64 | Max HTTP headers per request |
| `NP_MAX_URI_LEN` | 8192 | Max URI length |
| `NP_MAX_HEADER_LEN` | 8192 | Max single header length |
| `NP_MAX_BACKENDS` | 64 | Max upstream backends |
| `NP_ARENA_SIZE` | 64 KB | Per-connection arena size |
| `NP_READ_BUF_SIZE` | 64 KB | Read ring buffer size |
| `NP_WRITE_BUF_SIZE` | 128 KB | Write ring buffer size |
| `NP_MAX_WORKERS` | 64 | Max worker processes |
| `NP_EPOLL_EVENTS` | 1024 | Max events per epoll_wait call |
| `NP_TIMEOUT_BUCKETS` | 512 | Timeout wheel granularity |
