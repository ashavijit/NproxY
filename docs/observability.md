# Observability

Nproxy provides built-in observability through Prometheus metrics, a health check endpoint, and structured access/error logs.

---

## Health Check: `/healthz`

Always available (cannot be disabled). Returns a simple JSON response:

```bash
curl http://localhost:8080/healthz
```

```json
{"status":"ok"}
```

- Status code: `200 OK`
- Use this for load balancer health probes, Kubernetes liveness checks, or uptime monitors.

---

## Prometheus Metrics: `/metrics`

When enabled, Nproxy exposes a Prometheus-compatible text endpoint:

```ini
[metrics]
enabled = true
path    = /metrics
```

```bash
curl http://localhost:8080/metrics
```

### Available Metrics

| Metric | Type | Description |
|---|---|---|
| `nproxy_requests_total` | counter | Total HTTP requests received |
| `nproxy_requests_2xx_total` | counter | Responses with 2xx status |
| `nproxy_requests_4xx_total` | counter | Responses with 4xx status |
| `nproxy_requests_5xx_total` | counter | Responses with 5xx status |
| `nproxy_active_connections` | gauge | Currently open connections |
| `nproxy_upstream_errors_total` | counter | Failed upstream connection attempts |
| `nproxy_request_duration_seconds` | histogram | Request latency distribution |

### Histogram Buckets

The `nproxy_request_duration_seconds` histogram uses these bucket boundaries (in seconds):

```
0.0001, 0.0005, 0.001, 0.002, 0.005, 0.01,
0.02, 0.05, 0.1, 0.2, 0.5, 1.0,
2.0, 5.0, 10.0, +Inf
```

### Example Output

```
# HELP nproxy_requests_total Total HTTP requests
# TYPE nproxy_requests_total counter
nproxy_requests_total 154832
nproxy_requests_2xx_total 152109
nproxy_requests_4xx_total 2614
nproxy_requests_5xx_total 109
# HELP nproxy_active_connections Active connections
# TYPE nproxy_active_connections gauge
nproxy_active_connections 47
# HELP nproxy_upstream_errors_total Upstream errors
# TYPE nproxy_upstream_errors_total counter
nproxy_upstream_errors_total 12
# HELP nproxy_request_duration_seconds Request duration histogram
# TYPE nproxy_request_duration_seconds histogram
nproxy_request_duration_seconds_bucket{le="0.0001"} 89231
nproxy_request_duration_seconds_bucket{le="0.0005"} 120456
...
nproxy_request_duration_seconds_count 154832
nproxy_request_duration_seconds_sum 142.381920
```

### Prometheus Scrape Config

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'nproxy'
    scrape_interval: 15s
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: /metrics
```

### Grafana Dashboard

You can visualize Nproxy metrics in Grafana with queries like:

```promql
# Request rate
rate(nproxy_requests_total[1m])

# Error rate
rate(nproxy_requests_5xx_total[1m]) / rate(nproxy_requests_total[1m])

# p99 latency
histogram_quantile(0.99, rate(nproxy_request_duration_seconds_bucket[5m]))
```

### Per-Worker Note

Metrics counters are per-worker process. If you scrape from the outside (single endpoint), you get the counters for whichever worker handles the scrape request. For accurate global metrics, run Nproxy behind a metrics aggregator or scrape all workers individually.

---

## Access Log

**Source:** `src/features/access_log.c`

```ini
[log]
access_log = ./logs/access.log
```

Each request produces one log line in a format similar to the Nginx combined log format, with latency appended:

```
{client_ip} - - [{timestamp}] "{method} {path} HTTP/1.{version}" {status} {bytes} {latency}
```

**Example:**

```
203.0.113.42 - - [24/Feb/2026:19:16:50 +0000] "GET /api/hello HTTP/1.1" 200 312 47us
192.168.1.5  - - [24/Feb/2026:19:16:51 +0000] "POST /login HTTP/1.1" 302 0 1.2ms
```

---

## Error Log

```ini
[log]
level     = info
error_log = ./logs/error.log
```

The error log captures internal messages at the configured level:

| Level | Description |
|---|---|
| `error` | Failures: socket errors, upstream connect failures, config errors |
| `warn` | Non-critical issues: worker crashes, unhealthy backends |
| `info` | Startup, shutdown, worker spawn, config reload |
| `debug` | Verbose: per-connection state transitions, buffer sizes, parse details |

Set `error_log` to an empty string to log to stderr (useful in containers).

### Log Levels

Each level includes all levels above it:
- `debug` shows everything
- `info` shows info + warn + error
- `warn` shows warn + error
- `error` shows only errors

---

## Disabling Metrics

```ini
[metrics]
enabled = false
```

When disabled, no metrics structures are allocated and no CPU is spent on atomic counter updates.
