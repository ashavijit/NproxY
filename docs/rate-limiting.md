# Rate Limiting

Nproxy includes a built-in per-IP rate limiter using the **token bucket** algorithm. This protects backends from traffic spikes and abusive clients.

---

## Configuration

```ini
[rate_limit]
enabled             = true
requests_per_second = 1000
burst               = 200
```

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `true` | Enable rate limiting |
| `requests_per_second` | int | `1000` | Token refill rate (tokens added per second) |
| `burst` | int | `200` | Maximum bucket capacity (allows short bursts above the sustained rate) |

---

## How It Works

The [token bucket algorithm](https://en.wikipedia.org/wiki/Token_bucket) works as follows:

1. Each client IP gets a virtual "bucket" that holds up to `burst` tokens
2. Tokens are added at a rate of `requests_per_second` tokens per second
3. Each request consumes **1 token**
4. If the bucket is empty (< 1 token), the request is rejected with `429 Too Many Requests`
5. New clients start with a full bucket (`burst` tokens)

### Example

With `requests_per_second = 100` and `burst = 50`:

- A new client can immediately send **50 requests** (burst)
- After the burst, the client is limited to **100 requests/second** sustained
- If the client pauses, the bucket refills up to 50 tokens

---

## Response on Rate Limit

When a client exceeds the rate limit, Nproxy responds:

```
HTTP/1.1 429 Too Many Requests
Content-Length: 0
Connection: close
```

The request is logged in the access log with status `429` and counted in the `nproxy_requests_4xx_total` metric.

---

## Implementation Details

**Source:** `src/features/rate_limit.c`

- The rate limiter uses a fixed hash table of 4096 buckets (`RL_TABLE_SIZE`)
- Client IPs are hashed using FNV-1a to select a bucket
- Hash collisions are handled by overwriting (last-writer-wins) -- this is acceptable because the table is large relative to typical unique client counts
- Token refill is computed lazily: `tokens += elapsed_seconds * rate` on each check
- The rate limiter is per-worker (each worker has its own instance)

### Per-Worker Note

Because each worker has an independent rate limiter, the effective rate limit per IP is approximately `requests_per_second * worker_count`. For strict global rate limiting, set a lower per-worker rate:

```ini
# 4 workers, want 1000 req/s global limit per IP
# Set to 1000 / 4 = 250 per worker
[server]
worker_processes = 4

[rate_limit]
requests_per_second = 250
burst               = 50
```

---

## Disabling Rate Limiting

```ini
[rate_limit]
enabled = false
```

When disabled, no `rate_limiter_t` is created and the check is skipped entirely -- zero overhead.
