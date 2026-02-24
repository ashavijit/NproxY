# Contributing to Nproxy

## Getting Started

```bash
git clone https://github.com/youruser/nproxy.git
cd nproxy
make debug    # build with sanitizers
```

## Code Style

- C17, `_GNU_SOURCE` enabled
- 2-space indentation, 100-char line limit
- All public symbols prefixed with `np_` or module name (e.g. `conn_`, `buf_`, `log_`)
- No global mutable state outside carefully guarded singletons (logger, config)
- Arena-allocate per-request memory; never `malloc` in the hot path

## Project Layout

```
src/
  core/       types.h, memory.c, log.c, config.c, string_util.c
  net/        socket.c, buffer.c, event_loop.c, conn.c, timeout.c
  http/       parser.c, request.c, response.c, handler.c
  static/     mime.c, file_server.c
  proxy/      upstream.c, balancer.c, proxy_conn.c
  proc/       master.c, worker.c, signal.c
  tls/        tls_ctx.c, tls_conn.c
  features/   rate_limit.c, metrics.c, health.c, access_log.c
  main.c
```

## Adding a New Handler

1. Create `src/features/my_handler.{h,c}`
2. Add a `my_handle(conn_t *conn, http_request_t *req)` function
3. Register it in `handler_dispatch()` in `src/http/handler.c`
4. Add the `.o` to `SRCS` in `Makefile`

## Running the Test Backend

```bash
python3 test_backend.py 9000 &
python3 test_backend.py 9001 &
./nproxy -w                    # single-worker for easy debugging
```

## Debug Build

```bash
make debug
# Includes: -g -O0 -fsanitize=address,undefined
```

## Submitting Changes

1. Fork and create a feature branch: `git checkout -b feature/my-feature`
2. Keep commits atomic and write descriptive messages
3. Run a clean build before submitting: `make clean && make`
4. Open a pull request with a description of the change
