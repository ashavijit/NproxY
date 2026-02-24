# Nproxy Configuration Reference

Configuration is loaded from `nproxy.conf` (INI format, section-based).  
Override with: `nproxy -c /path/to/custom.conf`

---

## [server]

| Key | Type | Default | Description |
|---|---|---|---|
| `listen_addr` | string | `0.0.0.0` | IP address to bind |
| `listen_port` | int | `8080` | HTTP listen port |
| `worker_processes` | int | `4` | Number of worker processes. Set to CPU count for production |
| `backlog` | int | `4096` | TCP accept backlog |
| `max_connections` | int | `100000` | Max concurrent connections per worker |
| `keepalive_timeout` | int | `75` | Keep-alive idle timeout (seconds) |
| `read_timeout` | int | `60` | Client read timeout (seconds) |
| `write_timeout` | int | `60` | Client write timeout (seconds) |
| `static_root` | string | `./www` | Root directory for static file serving |

---

## [tls]

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable TLS listener |
| `listen_port` | int | `8443` | HTTPS listen port |
| `cert_file` | string | — | Path to PEM certificate |
| `key_file` | string | — | Path to PEM private key |

**Self-signed certificate for testing:**

```bash
mkdir -p certs
openssl req -x509 -newkey rsa:4096 -days 365 -nodes \
  -keyout certs/server.key \
  -out certs/server.crt \
  -subj "/CN=localhost"
```

---

## [proxy]

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable reverse proxy mode |
| `mode` | string | `round_robin` | Load balancing: `round_robin` or `least_conn` |
| `connect_timeout` | int | `5` | Upstream connect timeout (seconds) |
| `upstream_timeout` | int | `30` | Upstream response timeout (seconds) |

---

## [upstream]

Define one or more backends. All `backend` keys are collected into the upstream pool.

```ini
[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001
backend = 10.0.0.5:8000
```

Format: `host:port` (IPv4 or hostname)

---

## [rate_limit]

Token bucket algorithm, applied per client IP.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable rate limiting |
| `requests_per_second` | int | `1000` | Refill rate (tokens/second) |
| `burst` | int | `200` | Maximum burst size (bucket capacity) |

When the limit is exceeded, Nproxy returns `429 Too Many Requests`.

---

## [log]

| Key | Type | Default | Description |
|---|---|---|---|
| `level` | string | `info` | Log level: `error`, `warn`, `info`, `debug` |
| `access_log` | string | `./logs/access.log` | Access log file path (Combined Log Format + latency) |
| `error_log` | string | `./logs/error.log` | Error log file path (empty = stderr) |

**Access log format:**

```
{ip} - - [{time}] "{method} {path} HTTP/1.{ver}" {status} {bytes} {latency}
```

Example:
```
203.0.113.42 - - [24/Feb/2026:19:16:50 +0000] "GET /api/hello HTTP/1.1" 200 312 47us
```

---

## [metrics]

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Expose Prometheus metrics endpoint |
| `path` | string | `/metrics` | URL path for the metrics endpoint |

**Available metrics:**

| Metric | Type | Description |
|---|---|---|
| `nproxy_requests_total` | counter | Total requests received |
| `nproxy_requests_2xx_total` | counter | 2xx responses |
| `nproxy_requests_4xx_total` | counter | 4xx responses |
| `nproxy_requests_5xx_total` | counter | 5xx responses |
| `nproxy_active_connections` | gauge | Current open connections |
| `nproxy_upstream_errors_total` | counter | Failed upstream connections |
| `nproxy_request_duration_seconds` | histogram | Request latency distribution |

---

## Environment Variables

None. All configuration is via the config file and CLI flags.

---

## Full Example

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
