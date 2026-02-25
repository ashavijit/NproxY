<div align="center">
  <h1>Nproxy</h1>
  <img src="assets/nproxy.png" width="128" alt="Nproxy Logo">
  <br>
  <br>
</div>

A production-grade, event-driven HTTP reverse proxy and static file server built in C17.

```
      ┌──────────┐       ┌──────────┐       ┌──────────────┐
curl ─►  Nproxy  ├──RR──►  Backend  │       │ /healthz     │
      │  :8080   ├──RR──►  :9000    │       │ /metrics     │
      └──────────┘       └──────────┘       │ static files │
                                            └──────────────┘
```

## Features

| Feature | Detail |
|---|---|
| **Transport** | Non-blocking TCP, edge-triggered `epoll` |
| **Concurrency** | Master/worker fork model (Nginx-style) |
| **Memory** | Per-connection arena — zero `malloc` in hot path |
| **Proxy** | Round-robin & least-connections load balancing |
| **Static files** | `sendfile(2)` zero-copy, ETag, MIME |
| **TLS** | OpenSSL, TLS 1.2+, SNI |
| **Rate limiting** | Token bucket, per-IP |
| **Observability** | Prometheus `/metrics`, `/healthz`, combined access log |
| **Signals** | `signalfd`-based, graceful reload on `SIGHUP` |

---

## Missing vs Nginx

While Nproxy implements the core Nginx master/worker architecture and event loop, it omits legacy and complex features to remain under 5,000 lines of C:

- **Advanced Routing**: No regex `location` matching, no `rewrite` rules, no `try_files` fallbacks.
- **Dynamic Modules**: No loadable `.so` modules, Lua/OpenResty integration, or Perl bindings.
- **Protocol Support**: No HTTP/2, HTTP/3 (QUIC), FastCGI, uWSGI, or gRPC (only HTTP/1.1 proxying and static files).
- **Caching**: No built-in proxy cache or micro-caching (relies on downstream CDNs).
- **Compression**: No on-the-fly `gzip` or `brotli` compression.
- **Websockets**: No native `Upgrade` connection hijacking for bi-directional websockets yet.
- **Auth/ACLs**: No `auth_basic`, IP whitelisting (`allow`/`deny`), or complex request validation (besides simple rate-limiting).

---

## Quick Start

### 1. Install

```bash
curl -fsSL https://raw.githubusercontent.com/youruser/nproxy/main/install.sh | bash
```

Or build from source:

```bash
git clone https://github.com/ashavijit/NproxY.git
cd nproxy
bash install.sh --local          # install to ./bin/nproxy
```

### 2. Configure

```bash
cp nproxy.conf.example nproxy.conf
$EDITOR nproxy.conf
```

### 3. Run

```bash
# Test configuration
./nproxy -t

# Development (single worker, no fork)
./nproxy -w

# Production (multi-worker)
./nproxy

# Graceful config reload
kill -HUP $(cat /run/nproxy.pid)
```

---

## Build from Source

### Dependencies

| Dependency | Package (Debian/Ubuntu) | Package (Fedora/RHEL) |
|---|---|---|
| GCC 10+ or Clang 12+ | `build-essential` | `gcc make` |
| GNU Make | `make` | `make` |
| OpenSSL 1.1+ | `libssl-dev` | `openssl-devel` |

```bash
# Ubuntu / Debian
sudo apt install build-essential libssl-dev

# Fedora / RHEL / CentOS
sudo dnf install gcc make openssl-devel

# Arch Linux
sudo pacman -S base-devel openssl
```

### Compile

```bash
make            # optimized build
make debug      # debug build (-g -O0 -fsanitize=address)
make clean      # remove build artifacts
```

The binary is placed at `./nproxy` (≈70 KB, statically linked against libc).

---

## Configuration

The default config file is `nproxy.conf` (INI format). Pass a custom path with `-c`.

```ini
[server]
listen_port      = 8080
listen_addr      = 0.0.0.0
worker_processes = 4          # 0 = auto (number of CPU cores)
backlog          = 4096
keepalive_timeout = 75
read_timeout      = 60
write_timeout     = 60
static_root       = ./www     # serve files from here

[tls]
enabled     = false
listen_port = 8443
cert_file   = ./certs/server.crt
key_file    = ./certs/server.key

[proxy]
enabled          = true
mode             = round_robin   # round_robin | least_conn
connect_timeout  = 5
upstream_timeout = 30

[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001

[rate_limit]
enabled             = true
requests_per_second = 1000
burst               = 200

[log]
level      = info     # error | warn | info | debug
access_log = ./logs/access.log
error_log  = ./logs/error.log

[metrics]
enabled = true
path    = /metrics
```

> Full reference: see [CONFIGURATION.md](CONFIGURATION.md)

---

## Endpoints

| Path | Description |
|---|---|
| `/healthz` | Health check — `200 {"status":"ok"}` |
| `/metrics` | Prometheus text metrics |
| `/*` | Proxied to upstream pool (if `proxy.enabled = true`) |
| `/*` | Static file server (if `proxy.enabled = false`) |

---

## Systemd Service

```bash
sudo bash install.sh --systemd
sudo systemctl enable --now nproxy
```

Or manually:

```bash
sudo cp nproxy /usr/local/bin/
sudo cp nproxy.conf /etc/nproxy/nproxy.conf
sudo cp contrib/nproxy.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now nproxy
```

---

## CLI Reference

```
Usage: nproxy [-c config] [-t] [-w] [-v]

  -c <file>   Configuration file (default: nproxy.conf)
  -t          Test configuration and exit
  -w          Single worker mode (no fork — for development/debugging)
  -v          Print version and exit
```

---

## Architecture

```
master process
│
├── creates shared SO_REUSEPORT listen socket
├── forks N worker processes
├── monitors workers, respawns on crash
└── SIGHUP → graceful reload

worker process (×N)
│
├── epoll event loop (edge-triggered)
├── accept connections
├── HTTP/1.1 state machine
│   ├── parse request (zero-allocation)
│   ├── rate limit check
│   ├── route → health / metrics / proxy / static
│   └── flush response
└── timeout wheel (per-connection)
```

---

## Signals

| Signal | Action |
|---|---|
| `SIGTERM` | Graceful shutdown |
| `SIGINT` | Immediate shutdown |
| `SIGHUP` | Reload configuration (zero-downtime) |
| `SIGPIPE` | Ignored |

---

## License

MIT — see [LICENSE](LICENSE).
