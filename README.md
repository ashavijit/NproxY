<div align="center">
  <h1>Nproxy</h1>
  <img src="assets/nproxy.png" width="128" alt="Nproxy Logo">
  <br>
  <p><strong>Production-grade, event-driven HTTP reverse proxy & static file server in C17</strong></p>

  [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
  [![Build](https://img.shields.io/badge/Build-passing-brightgreen.svg)]()
  [![C17](https://img.shields.io/badge/C-17-orange.svg)]()
  [![Lines](https://img.shields.io/badge/Lines-~5K-lightgrey.svg)]()
</div>

```
      ┌──────────┐       ┌──────────┐       ┌──────────────┐
curl ─►  Nproxy  ├──RR──►  Backend  │       │ /healthz     │
      │  :8080   ├──RR──►  :9000    │       │ /metrics     │
      └──────────┘       └──────────┘       │ static files │
                                            └──────────────┘
```

---

## Install

### One-liner (build from source)

```bash
curl -fsSL https://raw.githubusercontent.com/ashavijit/NproxY/main/install.sh | sudo bash
```

### Local install (no root)

```bash
git clone https://github.com/ashavijit/NproxY.git && cd NproxY
bash install.sh --local
./bin/nproxy -c vhosts.conf -w
```

### Systemd service

```bash
git clone https://github.com/ashavijit/NproxY.git && cd NproxY
sudo bash install.sh --systemd
sudo systemctl enable --now nproxy
```

### Manual build

```bash
# Prerequisites
sudo apt install build-essential libssl-dev zlib1g-dev   # Debian/Ubuntu
sudo dnf install gcc make openssl-devel zlib-devel       # Fedora/RHEL
sudo pacman -S base-devel openssl zlib                   # Arch

# Build
make -j$(nproc)
./nproxy -t            # test config
./nproxy -w            # single-worker dev mode
```

---

## Features

| Feature | Detail |
|---|---|
| **Transport** | Non-blocking TCP, edge-triggered `epoll` |
| **Concurrency** | Master/worker fork model (Nginx-style) |
| **Memory** | Per-connection arena — zero `malloc` in hot path |
| **Proxy** | Round-robin & least-connections load balancing |
| **Caching** | Disk-backed HTTP response cache with TTL expiry |
| **Gzip** | zlib compression for text, JSON, JS, XML responses |
| **Static files** | `sendfile(2)` zero-copy, ETag, `try_files`, MIME |
| **TLS** | OpenSSL, TLS 1.2+, SNI |
| **Rate limiting** | Token bucket, per-IP |
| **Observability** | Prometheus `/metrics`, `/healthz`, combined access log |
| **Daemon mode** | Classic double-fork daemonization with PID file |
| **Hot reload** | `SIGHUP` — re-parse config, reconcile listeners, respawn workers |
| **Graceful shutdown** | Connection draining with configurable timeout |

---

## Quick Start

**1. Configure**

```bash
cp vhosts.conf my.conf
$EDITOR my.conf
```

**2. Run**

```bash
# Development (single worker, foreground)
./nproxy -c my.conf -w

# Production (multi-worker, foreground)
./nproxy -c my.conf

# Production (daemon mode)
./nproxy -c my.conf -d

# Reload config (zero-downtime)
kill -HUP $(cat /run/nproxy.pid)

# Graceful shutdown
kill -TERM $(cat /run/nproxy.pid)
```

---

## Configuration

Config file uses INI format. Pass custom path with `-c`.

```ini
[global]
worker_processes  = 4
max_connections   = 100000
keepalive_timeout = 75
shutdown_timeout  = 30

[server]
listen_port = 8080
server_name = example.com
static_root = ./www

[tls]
enabled     = false
listen_port = 8443
cert_file   = ./certs/server.crt
key_file    = ./certs/server.key

[proxy]
enabled          = true
mode             = round_robin    # round_robin | least_conn
connect_timeout  = 5
upstream_timeout = 30

[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001

[cache]
enabled     = true
root        = /tmp/nproxy_cache
default_ttl = 60
max_entries = 1024

[gzip]
enabled    = true
min_length = 256

[rate_limit]
enabled             = true
requests_per_second = 1000
burst               = 200

[log]
level      = info
access_log = ./logs/access.log
error_log  = ./logs/error.log

[metrics]
enabled = true
path    = /metrics

[process]
daemon   = false
pid_file = /run/nproxy.pid
```

---

## CLI Reference

```
Usage: nproxy [-c config] [-t] [-w] [-d] [-v]

  -c <file>   Configuration file (default: vhosts.conf)
  -t          Test configuration and exit
  -w          Single worker mode (no fork)
  -d          Daemonize (background mode with PID file)
  -v          Print version and exit
```

---

## Endpoints

| Path | Description |
|---|---|
| `/healthz` | Health check — `200 {"status":"ok"}` |
| `/metrics` | Prometheus text metrics |
| `/*` | Proxied to upstream pool (if `proxy.enabled = true`) |
| `/*` | Static file server (if `proxy.enabled = false`) |

---

## Architecture

```
master process
│
├── creates shared SO_REUSEPORT listen socket
├── forks N worker processes
├── monitors workers, respawns on crash
├── SIGHUP → reload config, reconcile listeners, respawn
└── SIGTERM → graceful shutdown with drain timeout

worker process (×N)
│
├── epoll event loop (edge-triggered)
├── accept connections
├── HTTP/1.1 state machine
│   ├── parse request (zero-allocation)
│   ├── rate limit check
│   ├── cache lookup (GET requests)
│   ├── route → health / metrics / proxy / static
│   ├── proxy: parse upstream status, cache insert on EOF
│   └── flush response
├── timeout wheel (per-connection)
└── graceful drain on shutdown
```

---

## Signals

| Signal | Action |
|---|---|
| `SIGTERM` | Graceful shutdown (drain connections) |
| `SIGINT` | Immediate shutdown |
| `SIGHUP` | Reload configuration (zero-downtime) |
| `SIGPIPE` | Ignored |

---

## License

MIT — see [LICENSE](LICENSE).
