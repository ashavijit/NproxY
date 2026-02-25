# TLS / HTTPS

Nproxy supports TLS 1.2+ termination via OpenSSL, allowing it to serve HTTPS traffic directly without an external TLS terminator.

---

## Enabling TLS

Add a `[tls]` block to your server configuration:

```ini
[server]
listen_port = 8080
static_root = ./www

[tls]
enabled     = true
listen_port = 8443
cert_file   = ./certs/server.crt
key_file    = ./certs/server.key
```

With this configuration, Nproxy listens on:
- **Port 8080** for plain HTTP
- **Port 8443** for HTTPS

---

## Certificate Setup

### Self-Signed (for development/testing)

```bash
mkdir -p certs
openssl req -x509 -newkey rsa:4096 -days 365 -nodes \
  -keyout certs/server.key \
  -out certs/server.crt \
  -subj "/CN=localhost"
```

Test it:

```bash
curl -k https://localhost:8443/healthz
```

### Let's Encrypt (for production)

Use `certbot` to obtain certificates, then point Nproxy at them:

```ini
[tls]
enabled     = true
listen_port = 443
cert_file   = /etc/letsencrypt/live/example.com/fullchain.pem
key_file    = /etc/letsencrypt/live/example.com/privkey.pem
```

After renewing certificates, reload Nproxy for changes to take effect:

```bash
kill -HUP $(pidof nproxy)
```

---

## SNI (Server Name Indication)

Nproxy supports SNI, which allows multiple TLS certificates on the same IP and port. When combined with [virtual hosts](virtual-hosts.md), the correct certificate is selected based on the hostname in the TLS handshake.

---

## TLS Configuration

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable TLS |
| `listen_port` | int | `8443` | Port for the HTTPS listener |
| `cert_file` | string | -- | Path to PEM certificate (or fullchain) |
| `key_file` | string | -- | Path to PEM private key |

---

## Implementation Details

- TLS context is managed in `src/tls/tls_ctx.c` (OpenSSL `SSL_CTX` initialization)
- Per-connection TLS state is in `src/tls/tls_conn.c`
- Nproxy uses OpenSSL's non-blocking I/O mode, integrated with the epoll event loop
- Minimum protocol version: TLS 1.2
