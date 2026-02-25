# Static File Serving

Nproxy includes a high-performance static file server that uses Linux's `sendfile(2)` for zero-copy file transfers and supports ETag-based caching.

---

## Basic Setup

Disable the proxy and set a `static_root`:

```ini
[server]
listen_port = 8080
static_root = ./www

[proxy]
enabled = false
```

Place files in the `www/` directory:

```bash
mkdir -p www
echo "<h1>Hello</h1>" > www/index.html
```

Nproxy will serve:
- `GET /` -> `www/index.html`
- `GET /style.css` -> `www/style.css`
- `GET /img/logo.png` -> `www/img/logo.png`

---

## Directory Index

When a request path ends with `/`, Nproxy automatically appends `index.html`:

- `GET /` -> `static_root/index.html`
- `GET /docs/` -> `static_root/docs/index.html`

---

## Zero-Copy Transfer with `sendfile(2)`

For file responses, Nproxy:

1. Writes the HTTP headers into the write buffer and flushes them
2. Uses `sendfile(2)` to transfer the file body directly from the filesystem to the socket

This means file contents **never enter userspace memory** -- the kernel transfers data directly from the page cache to the network stack. This is the fastest possible way to serve files on Linux.

---

## ETag Caching

Every file response includes an `ETag` header derived from the file's modification time and size:

```
ETag: "67bd3a1c-1a4f"
```

If a client sends `If-None-Match` with a matching ETag, Nproxy returns `304 Not Modified` with no body, saving bandwidth.

**Example:**

```
Request:
GET /style.css HTTP/1.1
If-None-Match: "67bd3a1c-1a4f"

Response:
HTTP/1.1 304 Not Modified
```

---

## MIME Types

Nproxy automatically detects the `Content-Type` from the file extension using a built-in MIME type table (`src/static/mime.c`). Common mappings include:

| Extension | Content-Type |
|---|---|
| `.html` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.json` | `application/json` |
| `.png` | `image/png` |
| `.jpg` / `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.woff2` | `font/woff2` |
| `.pdf` | `application/pdf` |

Unknown extensions default to `application/octet-stream`.

---

## `try_files` Directive

The `try_files` directive tells Nproxy to check multiple file paths before returning a 404. This is useful for single-page applications (SPAs) where all routes should fall back to `index.html`.

```ini
[server]
static_root = ./dist
try_files   = $uri $uri/index.html /index.html
```

The `$uri` variable is replaced with the request path. Nproxy tries each path in order and serves the first one that exists as a regular file.

**Example for an SPA:**

```ini
[server]
static_root = ./dist
try_files   = $uri /index.html

[proxy]
enabled = false
```

- `GET /style.css` -> tries `./dist/style.css` (found) -> serves it
- `GET /dashboard` -> tries `./dist/dashboard` (not found) -> tries `./dist/index.html` (found) -> serves it

---

## Path Safety

Nproxy rejects any request path containing `..` (directory traversal) and returns `403 Forbidden`. This prevents attackers from escaping the `static_root`.

---

## Static Files with Proxy

When the proxy is enabled, Nproxy routes to the upstream first. Static file serving only kicks in when `proxy.enabled = false` for that server block.

To serve both static files and proxy API requests, use virtual hosts or handle API routes through a module:

```ini
# Static site
[server]
listen_port = 8080
server_name = www.example.com
static_root = /var/www/html

[proxy]
enabled = false

# API proxy
[server]
listen_port = 8080
server_name = api.example.com

[proxy]
enabled = true

[upstream]
backend = 127.0.0.1:3000
```
