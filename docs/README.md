# Nproxy Documentation

Welcome to the Nproxy documentation. Nproxy is a production-grade, event-driven HTTP reverse proxy and static file server built in C17.

## Table of Contents

| Document | Description |
|---|---|
| [Getting Started](getting-started.md) | Installation, first run, and quick configuration |
| [Architecture](architecture.md) | Master/worker model, event loop, memory management, and request lifecycle |
| [Configuration Reference](configuration.md) | Complete reference for `nproxy.conf` with all sections and directives |
| [Reverse Proxy & Load Balancing](proxy.md) | Upstream pools, round-robin/least-conn balancing, connection keep-alive |
| [Static File Serving](static-files.md) | Zero-copy `sendfile`, ETag caching, MIME types, `try_files` |
| [TLS / HTTPS](tls.md) | Enabling TLS, certificate setup, SNI support |
| [Rate Limiting](rate-limiting.md) | Token-bucket algorithm, per-IP limiting, burst configuration |
| [Observability](observability.md) | Prometheus `/metrics`, `/healthz`, access/error logs |
| [Dynamic Modules](modules.md) | Writing and loading shared-object (`.so`) plugins |
| [Virtual Hosts](virtual-hosts.md) | Multi-domain hosting with `server_name` and Host-header routing |
| [URL Rewriting](rewriting.md) | Regex-based path rewriting with capture group substitution |
| [Deployment](deployment.md) | Systemd service, production tuning, Docker, and security hardening |
| [CLI Reference](cli.md) | Command-line flags and usage |
| [Benchmarks](benchmarks.md) | Performance comparison with Nginx and reproduction instructions |
| [Contributing](contributing.md) | Code style, project layout, and PR workflow |

## Quick Links

- **Source**: [github.com/ashavijit/NproxY](https://github.com/ashavijit/NproxY)
- **License**: MIT
