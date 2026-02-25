# Reverse Proxy & Load Balancing

Nproxy can act as a reverse proxy, forwarding client requests to one or more upstream backend servers and streaming the response back. This is the primary use case for Nproxy in production.

---

## Enabling the Proxy

```ini
[proxy]
enabled = true
mode = round_robin

[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001
```

When `proxy.enabled = true`, all requests that don't match a built-in endpoint (`/healthz`, `/metrics`) or a dynamic module are forwarded to the upstream pool.

---

## Load Balancing Algorithms

### Round-Robin (`round_robin`)

Requests are distributed evenly across all healthy backends in a circular order.

```ini
[proxy]
mode = round_robin
```

**Best for:** Homogeneous backends with similar capacity.

### Least Connections (`least_conn`)

Each request is sent to the backend with the fewest active connections at that moment.

```ini
[proxy]
mode = least_conn
```

**Best for:** Backends with varying response times or heterogeneous capacity.

---

## Upstream Health Tracking

Nproxy tracks backend health passively:

- Each backend starts as **healthy**
- On connection or proxy errors, the backend's `error_count` increments
- After **5 consecutive errors**, the backend is marked **unhealthy** and removed from the rotation
- A backend recovers automatically when a subsequent request succeeds with zero errors

This is a passive health check -- Nproxy only evaluates health based on real traffic. (Active health checks with periodic pings are on the roadmap.)

---

## Connection Keep-Alive to Upstreams

Nproxy maintains a pool of idle keep-alive connections to each backend to avoid TCP handshake overhead on every request:

```ini
[proxy]
keepalive_conns = 16
```

| Setting | Default | Description |
|---|---|---|
| `keepalive_conns` | `16` | Max idle connections to keep open per backend |

When a response completes, the upstream connection is returned to the idle pool (up to the limit). The next request to that backend reuses an idle connection instead of opening a new one.

---

## Proxy Headers

Nproxy automatically adds these headers when forwarding to upstreams:

| Header | Value |
|---|---|
| `X-Real-IP` | Client's IP address |
| `X-Forwarded-For` | Client's IP address |
| `Host` | Original `Host` header from the client |
| `Connection` | `keep-alive` (or `Upgrade` for WebSocket) |

All other client headers are forwarded unchanged (except `Connection`, which is rewritten).

---

## Timeouts

```ini
[proxy]
connect_timeout  = 5    # seconds to establish upstream TCP connection
upstream_timeout = 30   # seconds to wait for upstream response
```

| Timeout | Default | On Expiry |
|---|---|---|
| `connect_timeout` | 5s | Returns `502 Bad Gateway` |
| `upstream_timeout` | 30s | Returns `504 Gateway Timeout` |

---

## Error Responses

| Status | Condition |
|---|---|
| `502 Bad Gateway` | Cannot connect to any upstream backend |
| `503 Service Unavailable` | No healthy backends available in the pool |

---

## WebSocket Tunneling

When a client sends a request with the `Upgrade` header (e.g., WebSocket), Nproxy switches the connection to **tunnel mode** (`CONN_TUNNEL`). In this mode, data is bidirectionally streamed between the client and the selected upstream without HTTP framing -- raw TCP in both directions.

---

## Multi-Server Proxy Example

With virtual hosts, you can proxy different domains to different backend pools:

```ini
[server]
listen_port = 8080
server_name = api.example.com

[proxy]
enabled = true
mode = least_conn

[upstream]
backend = 10.0.1.10:3000
backend = 10.0.1.11:3000

[server]
listen_port = 8080
server_name = app.example.com

[proxy]
enabled = true
mode = round_robin

[upstream]
backend = 10.0.2.10:8000
backend = 10.0.2.11:8000
```

See [Virtual Hosts](virtual-hosts.md) for details on Host-header routing.

---

## How It Works Internally

1. `handler_dispatch()` determines the server block (via `Host` header matching)
2. `proxy_handle()` selects a backend using the configured balancer
3. `upstream_get_connection()` returns an idle keep-alive connection or opens a new one
4. `build_proxy_request()` constructs the HTTP/1.1 request with proxy headers
5. The connection enters `CONN_PROXYING` state
6. `proxy_on_upstream_event()` handles upstream I/O:
   - Writes the buffered request to the upstream
   - Reads the upstream response and streams it to the client
7. On completion, the upstream socket is returned to the idle pool or closed
