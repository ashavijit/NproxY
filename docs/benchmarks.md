# Benchmarks

This document summarizes Nproxy's performance characteristics compared to Nginx, based on real `wrk` load tests.

---

## Test Environment

| Component | Detail |
|---|---|
| **Machine** | ASUS ROG Strix G713RC, 16 cores, 14 GiB RAM |
| **OS / Kernel** | Linux 6.17.0-14-generic |
| **Nproxy build** | `gcc -O2 -march=native -std=c17`, single worker (`-w`) |
| **Nginx version** | nginx/1.24 (16 auto workers) |
| **Backend** | Python 3 `http.server` x2 on `:9000`/`:9001` |
| **Tool** | `wrk 4.1.0` |

---

## Results Summary

### Per-Worker Comparison

| Scenario | Nproxy (1 worker) | Nginx per worker (~16 workers) |
|---|---|---|
| Local handler `/healthz` (100 conns) | **106,669 RPS** | ~35,437 RPS |
| Local handler `/healthz` (500 conns) | **79,087 RPS** | ~46,898 RPS |
| Reverse proxy (100 conns) | **38,666 RPS** | ~810 RPS |

Nproxy achieves **~3x higher per-worker throughput** on local handler requests compared to Nginx.

---

## Test 1: Local Handler (`/healthz`)

Tests raw event-loop throughput with a tiny inline JSON response. No upstream I/O.

**100 connections:**

```
wrk -t4 -c100 -d10s http://127.0.0.1:8080/healthz
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| Requests/sec | **106,669** | 566,999 |
| Latency p50 | 0.89 ms | 0.09 ms |
| Latency p99 | 1.50 ms | 0.23 ms |
| Total requests (10s) | 1,077,339 | 5,670,190 |

**500 connections:**

```
wrk -t8 -c500 -d10s http://127.0.0.1:8080/healthz
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| Requests/sec | **79,087** | 750,373 |
| Latency p50 | 5.93 ms | 0.32 ms |
| Latency p99 | 10.47 ms | 3.71 ms |

---

## Test 2: Reverse Proxy

Tests the full proxy path: parse -> connect upstream -> forward -> stream response. Both proxying to the same 2x Python `http.server` backends.

```
wrk -t4 -c100 -d10s http://127.0.0.1:8080/api/bench
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| Requests/sec | **38,666** | 12,965 |
| Latency p50 | 1.12 ms | 0.78 ms |
| Latency p99 | 1,430 ms | 1,150 ms |
| Timeouts | 31 | 68 |

Nproxy's single-worker event loop achieves **3x more proxy requests** than Nginx with 16 workers. The Python backend is the bottleneck; Nproxy's non-blocking upstream model keeps the CPU utilized while waiting on slow backends.

---

## How to Reproduce

```bash
# 1. Start backends
python3 test_backend.py 9000 &
python3 test_backend.py 9001 &

# 2. Start Nproxy (single worker)
./nproxy -w &

# 3. Start Nginx (for comparison)
nginx -c $(pwd)/bench/nginx_bench.conf &

# 4. Run benchmarks
bash bench/run_bench.sh

# Results saved to /tmp/bench_results.txt
```

---

## Current Limitations

| Limitation | Root Cause | Impact |
|---|---|---|
| Single worker in `-w` mode | Development flag, no fork | Use `worker_processes = N` in production |
| No upstream keep-alive (historical) | New TCP connection per request | Now mitigated with `keepalive_conns` |
| Python backend bottleneck | GIL + single-threaded server | Use a faster backend for realistic benchmarks |

---

## Further Reading

- [BENCHMARK.md](../BENCHMARK.md) -- full raw `wrk` output with detailed analysis
- [Architecture](architecture.md) -- understand the event loop and connection model
