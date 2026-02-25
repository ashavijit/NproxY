# Virtual Hosts

Nproxy supports multiple virtual hosts (server blocks) on the same listen port, routing requests to different backends or static roots based on the `Host` header.

---

## How It Works

1. The client sends an HTTP request with a `Host` header (e.g., `Host: api.example.com`)
2. Nproxy strips the port from the header and compares against each server block's `server_name`
3. The first matching server block handles the request
4. If no `server_name` matches, the **first server block** is used as the default

---

## Configuration

Define multiple `[server]` blocks in a single config file. Each block can have its own `server_name`, `static_root`, `[proxy]`, `[upstream]`, `[tls]`, modules, and rewrite rules.

```ini
# Default / fallback server
[server]
listen_port = 8080
server_name = www.example.com
static_root = /var/www/html

[proxy]
enabled = false

# API server
[server]
listen_port = 8080
server_name = api.example.com

[proxy]
enabled = true
mode = least_conn

[upstream]
backend = 10.0.1.10:3000
backend = 10.0.1.11:3000

# Admin panel
[server]
listen_port = 8080
server_name = admin.example.com

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:4000
```

All three virtual hosts share port 8080. The `Host` header determines which server block handles each request.

---

## Testing Virtual Hosts Locally

Use curl's `Host` header override:

```bash
# Route to api.example.com server block
curl -H "Host: api.example.com" http://localhost:8080/

# Route to admin.example.com server block
curl -H "Host: admin.example.com" http://localhost:8080/

# No match → falls back to first server block (www.example.com)
curl -H "Host: unknown.example.com" http://localhost:8080/
```

Or add entries to `/etc/hosts` for local testing:

```
127.0.0.1  www.example.com api.example.com admin.example.com
```

---

## Virtual Hosts with TLS

Each server block can have its own TLS certificate. Combined with SNI, Nproxy selects the correct certificate based on the hostname in the TLS handshake:

```ini
[server]
listen_port = 8080
server_name = site-a.com

[tls]
enabled     = true
listen_port = 8443
cert_file   = /etc/certs/site-a.com/fullchain.pem
key_file    = /etc/certs/site-a.com/privkey.pem

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:8081

[server]
listen_port = 8080
server_name = site-b.com

[tls]
enabled     = true
listen_port = 8443
cert_file   = /etc/certs/site-b.com/fullchain.pem
key_file    = /etc/certs/site-b.com/privkey.pem

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:8082
```

---

## Example: `vhosts.conf`

The repo includes an example virtual hosts config:

```ini
[global]
worker_processes = 2
max_connections = 4096
keepalive_timeout = 65

[server]
listen_port = 8080
server_name = a.local

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:8081

[server]
listen_port = 8080
server_name = b.local

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:8082
```

---

## Limits

- Maximum server blocks: **16** (`CONFIG_MAX_SERVERS`)
- If multiple server blocks share the same `listen_port`, Nproxy binds the port once and shares the listener across all matching blocks
- `server_name` matching is exact (case-sensitive). Wildcard and regex matching are not supported
