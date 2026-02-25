# Configuration Reference

Nproxy reads its configuration from an INI-format file (default: `nproxy.conf`).
Override the path with `-c`:

```bash
nproxy -c /etc/nproxy/nproxy.conf
```

---

## File Format

- Sections are denoted by `[section_name]`
- Key-value pairs use `key = value` syntax
- Comments start with `#`
- Boolean values accept: `true`, `1`, `yes` (anything else is false)
- Multiple `[server]` blocks are supported for virtual hosting

---

## [server]

Global server settings and per-server-block overrides.

| Key | Type | Default | Description |
|---|---|---|---|
| `listen_addr` | string | `0.0.0.0` | IP address to bind (global) |
| `listen_port` | int | `8080` | HTTP listen port |
| `server_name` | string | *(empty)* | Virtual host name for Host-header routing |
| `worker_processes` | int | `4` | Number of worker processes (global). Set to CPU core count for production |
| `backlog` | int | `4096` | TCP accept backlog (global) |
| `max_connections` | int | `100000` | Max concurrent connections per worker (global) |
| `keepalive_timeout` | int | `75` | Keep-alive idle timeout in seconds (global) |
| `read_timeout` | int | `60` | Client read timeout in seconds (global) |
| `write_timeout` | int | `60` | Client write timeout in seconds (global) |
| `static_root` | string | `./www` | Root directory for static file serving (per-server) |
| `load_module` | string | *(none)* | Path to a `.so` dynamic module (repeatable, per-server) |
| `rewrite` | string | *(none)* | Rewrite rule: `<regex> <replacement>` (repeatable, per-server) |
| `try_files` | string | *(none)* | Space-separated file paths to try, with `$uri` substitution (per-server) |

---

## [tls]

TLS configuration (per-server block).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable the TLS listener |
| `listen_port` | int | `8443` | HTTPS listen port |
| `cert_file` | string | *(required if enabled)* | Path to PEM-encoded certificate file |
| `key_file` | string | *(required if enabled)* | Path to PEM-encoded private key file |

Generate a self-signed certificate for testing:

```bash
mkdir -p certs
openssl req -x509 -newkey rsa:4096 -days 365 -nodes \
  -keyout certs/server.key \
  -out certs/server.crt \
  -subj "/CN=localhost"
```

---

## [proxy]

Reverse proxy settings (per-server block).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable reverse proxy mode |
| `mode` | string | `round_robin` | Load balancing algorithm: `round_robin` or `least_conn` |
| `connect_timeout` | int | `5` | Timeout for connecting to upstream (seconds) |
| `upstream_timeout` | int | `30` | Timeout for upstream response (seconds) |
| `keepalive_conns` | int | `16` | Max idle keep-alive connections per upstream backend |

---

## [upstream]

Define backend servers for the upstream pool (per-server block).

```ini
[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001
backend = 10.0.0.5:8000
```

| Key | Type | Description |
|---|---|---|
| `backend` | string | `host:port` of an upstream backend. Repeatable -- all entries form the pool |

- Format: `host:port` (IPv4 or hostname)
- If the port is omitted, it defaults to `80`
- Max backends per server block: 64 (`CONFIG_MAX_BACKENDS`)

---

## [rate_limit]

Token-bucket rate limiting, applied per client IP (global).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable rate limiting |
| `requests_per_second` | int | `1000` | Token refill rate (tokens per second) |
| `burst` | int | `200` | Maximum burst size (bucket capacity) |

When a client exceeds the limit, Nproxy returns `429 Too Many Requests`.

---

## [log]

Logging configuration (global).

| Key | Type | Default | Description |
|---|---|---|---|
| `level` | string | `info` | Log level: `error`, `warn`, `info`, `debug` |
| `access_log` | string | `./logs/access.log` | Path to the access log file (Combined Log Format + latency) |
| `error_log` | string | `./logs/error.log` | Path to the error log file. Empty string = stderr |

**Access log format:**

```
{ip} - - [{time}] "{method} {path} HTTP/1.{ver}" {status} {bytes} {latency}
```

Example entry:

```
203.0.113.42 - - [24/Feb/2026:19:16:50 +0000] "GET /api/hello HTTP/1.1" 200 312 47us
```

---

## [metrics]

Prometheus-compatible metrics endpoint (global).

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable the metrics endpoint |
| `path` | string | `/metrics` | URL path for the metrics endpoint |

See [Observability](observability.md) for the full list of exposed metrics.

---

## Environment Variables

None. All configuration is via the config file and CLI flags.

---

## Complete Example

```ini
[server]
listen_port       = 80
listen_addr       = 0.0.0.0
worker_processes  = 8
backlog           = 8192
max_connections   = 200000
keepalive_timeout = 75
read_timeout      = 30
write_timeout     = 30
static_root       = /var/www/html

[tls]
enabled     = true
listen_port = 443
cert_file   = /etc/nproxy/certs/fullchain.pem
key_file    = /etc/nproxy/certs/privkey.pem

[proxy]
enabled          = true
mode             = least_conn
connect_timeout  = 3
upstream_timeout = 15
keepalive_conns  = 32

[upstream]
backend = 10.0.1.10:3000
backend = 10.0.1.11:3000
backend = 10.0.1.12:3000

[rate_limit]
enabled             = true
requests_per_second = 500
burst               = 100

[log]
level      = warn
access_log = /var/log/nproxy/access.log
error_log  = /var/log/nproxy/error.log

[metrics]
enabled = true
path    = /metrics
```

---

## Limits

| Limit | Value | Source |
|---|---|---|
| Max server blocks | 16 | `CONFIG_MAX_SERVERS` |
| Max backends per server | 64 | `CONFIG_MAX_BACKENDS` |
| Max loaded modules per server | 16 | `CONFIG_MAX_MODULES` |
| Max rewrite rules per server | 32 | `CONFIG_MAX_REWRITES` |
| Max try_files entries per server | 8 | `CONFIG_MAX_TRY_FILES` |
| Max string length (paths, names) | 512 chars | `CONFIG_MAX_STR` |
