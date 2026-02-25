# Contributing

Thank you for your interest in contributing to Nproxy! This guide covers the development workflow, code conventions, and how to submit changes.

---

## Getting Started

```bash
git clone https://github.com/ashavijit/NproxY.git
cd NproxY
make debug    # build with AddressSanitizer
```

---

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
  module/     module.c
  features/   rate_limit.c, metrics.c, health.c, access_log.c
  main.c
```

See [Architecture](architecture.md) for a detailed walkthrough of each module.

---

## Code Style

- **Language:** C17 with `_GNU_SOURCE` enabled
- **Indentation:** 2 spaces (no tabs)
- **Line limit:** 100 characters
- **Symbol naming:** All public symbols prefixed with `np_` or a module-specific prefix (`conn_`, `buf_`, `log_`, `upstream_`, etc.)
- **Global state:** Avoided. Exceptions are carefully guarded singletons (logger, config, module registry)
- **Memory:** Arena-allocate per-request memory. Never `malloc` in the hot path
- **Formatting:** Run `make format` (uses `clang-format`) before committing

```bash
make format
```

---

## Build Targets

| Target | Description |
|---|---|
| `make` | Optimized release build (`-O2 -march=native`) |
| `make debug` | Debug build with `-g -O0 -fsanitize=address,undefined` |
| `make clean` | Remove all build artifacts |
| `make format` | Auto-format all `.c` and `.h` files |

---

## Running the Test Backend

For testing the reverse proxy:

```bash
python3 test_backend.py 9000 &
python3 test_backend.py 9001 &
./nproxy -w    # single worker for easy debugging
```

Then send requests:

```bash
curl http://localhost:8080/healthz
curl http://localhost:8080/api/bench
curl http://localhost:8080/metrics
```

---

## Adding a New Handler

1. Create `src/features/my_handler.{h,c}`
2. Implement `my_handle(conn_t *conn, http_request_t *req)`:
   - Write response using `response_write_simple()` or `response_write_error()`
   - Set `conn->state = CONN_WRITING_RESPONSE`
3. Register the handler in `handler_dispatch()` in `src/http/handler.c`
4. The Makefile's `find` command auto-discovers new `.c` files -- no Makefile changes needed

---

## Adding a New Module

See [Dynamic Modules](modules.md) for the full API reference. Quick steps:

1. Create `my_module.c` with an exported `nproxy_module` symbol
2. Compile: `gcc -shared -fPIC -o my_module.so my_module.c -Isrc -std=c17 -D_GNU_SOURCE`
3. Add `load_module = ./my_module.so` to your config
4. Test with `./nproxy -w`

---

## Submitting Changes

1. **Fork** the repository on GitHub
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes, keeping commits atomic with descriptive messages
4. Run a clean build: `make clean && make`
5. Run the debug build to catch memory bugs: `make clean && make debug && ./nproxy -t`
6. Format your code: `make format`
7. Open a pull request with a description of the change

---

## Reporting Issues

When filing an issue, please include:
- OS and kernel version (`uname -a`)
- GCC/Clang version (`gcc --version`)
- OpenSSL version (`openssl version`)
- Steps to reproduce
- Relevant log output (set `level = debug` for verbose logs)
