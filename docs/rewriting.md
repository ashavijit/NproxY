# URL Rewriting

Nproxy supports regex-based URL rewriting that transforms request paths before they reach the handler chain. This is useful for clean URLs, API versioning, and legacy path migration.

---

## Configuration

Add `rewrite` directives to a `[server]` block:

```ini
[server]
listen_port = 8080
rewrite = ^/old-path/(.*)  /new-path/$1
rewrite = ^/v1/api/(.*)    /v2/api/$1
```

### Syntax

```
rewrite = <regex_pattern> <replacement>
```

- **`regex_pattern`**: A POSIX Extended Regular Expression (ERE) matched against the request path
- **`replacement`**: The replacement string. Capture groups `$0` through `$9` are substituted

---

## How It Works

1. Rewrite rules are evaluated **in order** for the matched server block
2. The **first matching rule** rewrites the path; subsequent rules are skipped
3. The rewritten path is used for all downstream routing (modules, rate limiting, proxy, static files)
4. The rewrite happens in-place using arena-allocated memory (zero heap allocations)

---

## Examples

### Clean URLs for a static site

Rewrite `/about` to `/about.html`:

```ini
rewrite = ^/([a-z-]+)$  /$1.html
```

- `/about` -> `/about.html`
- `/contact` -> `/contact.html`
- `/blog/post` -> no match (contains `/`)

### API versioning

Redirect all v1 API calls to v2:

```ini
rewrite = ^/api/v1/(.*)  /api/v2/$1
```

- `/api/v1/users` -> `/api/v2/users`
- `/api/v1/orders/123` -> `/api/v2/orders/123`

### Strip a prefix

Remove `/app` prefix before proxying:

```ini
rewrite = ^/app/(.*)  /$1
```

- `/app/dashboard` -> `/dashboard`
- `/app/settings/profile` -> `/settings/profile`

### Capture groups

Multiple capture groups are supported (`$1` through `$9`):

```ini
rewrite = ^/users/([0-9]+)/posts/([0-9]+)  /api/posts?user=$1&id=$2
```

- `/users/42/posts/7` -> `/api/posts?user=42&id=7`

---

## Per-Server-Block Scope

Rewrite rules are scoped to the server block they appear in. Different virtual hosts can have different rewrite rules:

```ini
[server]
server_name = api.example.com
rewrite = ^/v1/(.*)  /v2/$1

[server]
server_name = www.example.com
rewrite = ^/blog/([0-9]+)  /posts/$1
```

---

## Limits

| Limit | Value |
|---|---|
| Max rewrite rules per server block | 32 (`CONFIG_MAX_REWRITES`) |
| Max path length after rewrite | 1024 characters |
| Regex flavor | POSIX Extended Regular Expressions (ERE) |
| Max capture groups | 10 (`$0` through `$9`) |

---

## Debugging Rewrites

Enable debug logging to see rewrite activity:

```ini
[log]
level = debug
```

Invalid regex patterns are logged as errors at startup and silently skipped.

---

## Interaction with Other Features

Rewrite rules are applied **after** module handlers and **before** the rate limiter and all other routing:

```
1. Dynamic modules       (may handle before rewrite)
2. ► URL rewrite rules   (path is modified here)
3. Rate limiter          (sees rewritten path)
4. /metrics
5. /healthz
6. Proxy / static files  (sees rewritten path)
```
