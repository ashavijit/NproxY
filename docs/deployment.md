# Deployment

This guide covers running Nproxy in production with systemd, Docker, and recommended tuning.

---

## Systemd Service

### Automated Install

```bash
sudo bash install.sh --systemd
sudo systemctl enable --now nproxy
```

This:
1. Compiles Nproxy from source
2. Installs the binary to `/usr/local/bin/nproxy`
3. Creates the `nproxy` system user
4. Installs the config to `/etc/nproxy/nproxy.conf`
5. Creates log and PID directories
6. Installs and enables the systemd unit

### Manual Install

```bash
# Build
make

# Install binary
sudo install -Dm755 nproxy /usr/local/bin/nproxy

# Install config
sudo mkdir -p /etc/nproxy
sudo cp nproxy.conf /etc/nproxy/nproxy.conf

# Install service
sudo cp contrib/nproxy.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now nproxy
```

### Managing the Service

```bash
# Start / stop / restart
sudo systemctl start nproxy
sudo systemctl stop nproxy
sudo systemctl restart nproxy

# Graceful config reload (SIGHUP)
sudo systemctl reload nproxy

# Check status
sudo systemctl status nproxy

# View logs
sudo journalctl -u nproxy -f
```

### Systemd Unit Details

The provided unit file (`contrib/nproxy.service`) includes security hardening:

```ini
[Service]
Type=forking
User=nproxy
Group=nproxy
ExecStart=/usr/local/bin/nproxy -c /etc/nproxy/nproxy.conf
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5s

# Security hardening
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/var/log/nproxy /run/nproxy
PrivateTmp=yes
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_BIND_SERVICE
```

`CAP_NET_BIND_SERVICE` allows binding to privileged ports (80, 443) without running as root.

---

## Docker

Create a `Dockerfile`:

```dockerfile
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y build-essential libssl-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN make clean && make -j$(nproc)

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y libssl3 && rm -rf /var/lib/apt/lists/*
RUN useradd --system --no-create-home nproxy

COPY --from=builder /build/nproxy /usr/local/bin/nproxy
COPY nproxy.conf /etc/nproxy/nproxy.conf

RUN mkdir -p /var/log/nproxy && chown nproxy:nproxy /var/log/nproxy

USER nproxy
EXPOSE 8080 8443

CMD ["nproxy", "-c", "/etc/nproxy/nproxy.conf", "-w"]
```

Build and run:

```bash
docker build -t nproxy .
docker run -d -p 8080:8080 -p 8443:8443 \
  -v $(pwd)/nproxy.conf:/etc/nproxy/nproxy.conf:ro \
  --name nproxy nproxy
```

> **Note:** Use `-w` (single worker) in containers, or set `worker_processes` to the number of allocated CPUs.

---

## Production Configuration

### Recommended `nproxy.conf`

```ini
[server]
listen_port       = 80
listen_addr       = 0.0.0.0
worker_processes  = 0          # 0 = auto-detect CPU count
backlog           = 8192
max_connections   = 200000
keepalive_timeout = 75
read_timeout      = 30
write_timeout     = 30

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

## OS Tuning

### File Descriptor Limits

Each connection uses one fd (two for proxied connections). Increase limits:

```bash
# /etc/security/limits.conf
nproxy soft nofile 1048576
nproxy hard nofile 1048576
```

Or in the systemd unit:

```ini
[Service]
LimitNOFILE=1048576
```

### TCP Tuning

```bash
# /etc/sysctl.conf
net.core.somaxconn = 65535
net.core.netdev_max_backlog = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.tcp_tw_reuse = 1
net.ipv4.ip_local_port_range = 1024 65535
net.ipv4.tcp_fin_timeout = 15
```

Apply: `sudo sysctl -p`

### Transparent Huge Pages

Disable THP to avoid latency spikes:

```bash
echo never > /sys/kernel/mm/transparent_hugepage/enabled
```

---

## Log Rotation

Use `logrotate` to manage log file sizes:

```
# /etc/logrotate.d/nproxy
/var/log/nproxy/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    postrotate
        kill -USR1 $(cat /run/nproxy/nproxy.pid 2>/dev/null) 2>/dev/null || true
    endscript
}
```

---

## Uninstalling

```bash
sudo bash install.sh --uninstall
```

This stops the service, removes the binary, config directory, systemd unit, and system user. Log files at `/var/log/nproxy/` are preserved.

---

## Security Checklist

- [ ] Run as a dedicated `nproxy` user (not root)
- [ ] Use `CAP_NET_BIND_SERVICE` for ports 80/443 instead of root
- [ ] Enable `ProtectSystem=strict` in systemd
- [ ] Set appropriate file descriptor limits
- [ ] Enable rate limiting to mitigate DDoS
- [ ] Use TLS with strong certificates
- [ ] Restrict the metrics endpoint to internal networks if possible
- [ ] Rotate logs to prevent disk exhaustion
- [ ] Monitor with Prometheus and set alerts on `5xx` rate and upstream errors
