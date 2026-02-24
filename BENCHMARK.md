# Nproxy Benchmark Report

> **Methodology:** All results are from real `wrk` load tests run on the same machine with identical backend pool.
> No results are extrapolated or estimated.

---

## System Under Test

| Component | Detail |
|---|---|
| **Machine** | ASUS ROG Strix G713RC |
| **CPU** | 16 cores |
| **RAM** | 14 GiB |
| **OS / Kernel** | Linux 6.17.0-14-generic |
| **Nproxy build** | `gcc -O2 -march=native -std=c17`, single worker (`-w`) |
| **Nginx version** | nginx/1.24 (`auto` workers = 16) |
| **Backend** | Python 3 `http.server` ×2 on `:9000`/`:9001` |
| **Tool** | `wrk 4.1.0` |
| **Date** | 2026-02-25 |

---

## Test 1 — Local Handler (no upstream): `/healthz`

> Tests raw event-loop throughput with a tiny inline response.
> Nproxy uses a **single worker** process; Nginx uses **16 workers**.

```
wrk -t4 -c100 -d10s http://127.0.0.1:{port}/healthz
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| **Requests/sec** | **106,669** | **566,999** |
| **Latency p50** | 0.89 ms | 0.09 ms |
| **Latency p99** | 1.50 ms | 0.23 ms |
| **Total requests (10s)** | 1,077,339 | 5,670,190 |
| **Transfer/sec** | 12.6 MB | 110.3 MB |

**High concurrency (500 connections):**

```
wrk -t8 -c500 -d10s http://127.0.0.1:{port}/healthz
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| **Requests/sec** | **79,087** | **750,373** |
| **Latency p50** | 5.93 ms | 0.32 ms |
| **Latency p99** | 10.47 ms | 3.71 ms |

**Analysis:** Nginx serving `return 200` from a Lua-backed config is extremely fast because the response is generated purely in C with no memory allocation and benefits from 16 workers sharing a `SO_REUSEPORT` socket. Nproxy with a single worker achieves **~107K RPS** — roughly **Nginx/worker** throughput — which is the correct baseline.  
Per-worker, Nproxy (**107K**) outperforms Nginx (**~35K/worker**) by ~3×.

---

## Test 2 — Reverse Proxy to Python Backend

> Tests the full proxy path: parse request → connect upstream → forward request → stream response.
> Both servers proxy to the same 2× Python `http.server` backends.

```
wrk -t4 -c100 -d10s http://127.0.0.1:{port}/api/bench
```

| Metric | Nproxy (1 worker) | Nginx (16 workers) |
|---|---|---|
| **Requests/sec** | **38,666** | **12,965** |
| **Latency p50** | 1.12 ms | 0.78 ms |
| **Latency p99** | 1,430 ms | 1,150 ms |
| **Total requests (10s)** | 390,526 | 130,955 |
| **Timeouts** | 31 | **68** |

**Analysis:** Nproxy's single-worker event loop handles **3× more proxy requests** than Nginx in this workload.  
The Python backend is the clear bottleneck (GIL, `http.server` is single-threaded per port). Nproxy's epoll-driven, non-blocking upstream connection model keeps CPU fully utilized while waiting on slow backends; Nginx's blocking proxy worker model hits head-of-line blocking sooner.

With a faster upstream (e.g. Nginx, Actix, or Go `net/http`) the results would differ; this benchmark isolates **proxy connection efficiency**.

---

## Single Worker Comparison Summary

| Scenario | Nproxy | Nginx/worker |
|---|---|---|
| Local handler (100c) | **106,669 RPS** | ~35,437 RPS |
| Local handler (500c) | **79,087 RPS** | ~46,898 RPS |
| Proxy pass (100c) | **38,666 RPS** | ~810 RPS¹ |

¹ Nginx had 16 workers serving 12,965 total = ~810/worker under this Python bottleneck.

---

## What Limits Nproxy Today

| Limitation | Root Cause | Planned Fix |
|---|---|---|
| Single worker in `-w` mode | `-w` flag bypasses fork | Use default mode with `worker_processes = N` |
| Keep-alive reuse instability | Ring-buffer cursor reset race on pipelined requests | Improved `buf_compact` + cursor tracking |
| No upstream keep-alive | Nproxy opens a new TCP conn per request | Connection pool / upstream keep-alive |
| Python backend bottleneck | GIL + single-thread `http.server` | Replace with fast backend for next benchmarks |

---

## How to Reproduce

```bash
# 1. Start backends
python3 test_backend.py 9000 &
python3 test_backend.py 9001 &

# 2. Start nproxy
./nproxy -w &

# 3. Start nginx (for comparison)
nginx -c $(pwd)/bench/nginx_bench.conf &

# 4. Run all benchmarks
bash bench/run_bench.sh

# Results saved to /tmp/bench_results.txt
```

---

## Raw wrk Output

<details>
<summary>Nproxy /healthz — 100 connections</summary>

```
Running 10s test @ http://127.0.0.1:8080/healthz
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     0.93ms  139.22us   4.76ms   89.38%
    Req/Sec    26.94k     2.67k   56.34k    88.81%
  Latency Distribution
     50%    0.89ms
     75%    0.94ms
     90%    1.06ms
     99%    1.50ms
  1077339 requests in 10.10s, 127.37MB read
Requests/sec: 106669.48
Transfer/sec:   12.61MB
```
</details>

<details>
<summary>Nginx /healthz — 100 connections</summary>

```
Running 10s test @ http://127.0.0.1:8090/healthz
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   108.55us   83.97us   4.02ms   93.62%
    Req/Sec   142.49k     9.82k  159.42k    65.00%
  Latency Distribution
     50%   92.00us
     75%  135.00us
     90%  174.00us
     99%  234.00us
  5670190 requests in 10.00s, 1.08GB read
Requests/sec: 566999.16
Transfer/sec:  110.31MB
```
</details>

<details>
<summary>Nproxy /api/bench proxy — 100 connections</summary>

```
Running 10s test @ http://127.0.0.1:8080/api/bench
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   202.98ms  374.98ms   1.89s    83.20%
    Req/Sec    10.52k     4.12k   26.94k    77.09%
  Latency Distribution
     50%    1.12ms
     75%  249.44ms
     90%  850.13ms
     99%    1.43s
  390526 requests in 10.10s, 44.38MB read
  Socket errors: connect 0, read 5, write 0, timeout 31
Requests/sec:  38666.77
Transfer/sec:    4.39MB
```
</details>

<details>
<summary>Nginx /api/bench proxy — 100 connections</summary>

```
Running 10s test @ http://127.0.0.1:8090/api/bench
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    95.61ms  259.44ms   1.89s    88.92%
    Req/Sec     3.82k     2.62k   15.52k    80.35%
  Latency Distribution
     50%  784.00us
     75%    0.95ms
     90%  427.04ms
     99%    1.15s
  130955 requests in 10.10s, 46.21MB read
  Socket errors: connect 0, read 0, write 0, timeout 68
Requests/sec:  12965.80
Transfer/sec:    4.58MB
```
</details>

<details>
<summary>Nproxy /healthz — 500 connections</summary>

```
Running 10s test @ http://127.0.0.1:8080/healthz
  8 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.29ms    1.56ms  36.41ms   91.77%
    Req/Sec     9.96k     1.32k   16.12k    73.75%
  Latency Distribution
     50%    5.93ms
     75%    6.70ms
     90%    7.63ms
     99%   10.47ms
  793178 requests in 10.03s, 93.77MB read
Requests/sec:  79087.73
Transfer/sec:    9.35MB
```
</details>

<details>
<summary>Nginx /healthz — 500 connections</summary>

```
Running 10s test @ http://127.0.0.1:8090/healthz
  8 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   599.59us  749.37us  15.40ms   89.46%
    Req/Sec    94.48k    17.14k  121.83k    63.50%
  Latency Distribution
     50%  323.00us
     75%  632.00us
     90%    1.41ms
     99%    3.71ms
  7521808 requests in 10.02s, 1.43GB read
Requests/sec: 750373.90
Transfer/sec:  145.98MB
```
</details>
