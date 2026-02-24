#!/usr/bin/env bash
# Nproxy benchmark runner
# Runs wrk against Nproxy and Nginx, captures results

set -euo pipefail

DURATION=10      # seconds per run
CONNECTIONS=100
THREADS=4
WRK="wrk"

NPROXY="http://127.0.0.1:8080"
NGINX="http://127.0.0.1:8090"

OUT="/tmp/bench_results.txt"
> "$OUT"

log() {
  echo "  $*" | tee -a "$OUT"
}

run_bench() {
  local label="$1"
  local url="$2"
  log ">>> $label"
  $WRK -t$THREADS -c$CONNECTIONS -d${DURATION}s --latency "$url" 2>&1 | tee -a "$OUT"
  log ""
}

echo "========================================"  | tee -a "$OUT"
echo " Nproxy Benchmark Results"               | tee -a "$OUT"
echo " $(date '+%Y-%m-%d %H:%M:%S')"          | tee -a "$OUT"
echo " CPU: $(nproc) cores  RAM: $(free -h | awk '/Mem/{print $2}')" | tee -a "$OUT"
echo " Kernel: $(uname -r)"                   | tee -a "$OUT"
echo "========================================"  | tee -a "$OUT"
echo ""                                          | tee -a "$OUT"

echo "=== TEST 1: Health check (no upstream) ===" | tee -a "$OUT"
run_bench "Nproxy  /healthz  ($CONNECTIONS conn, ${DURATION}s)" "$NPROXY/healthz"
run_bench "Nginx   /healthz  ($CONNECTIONS conn, ${DURATION}s)" "$NGINX/healthz"

echo ""                                           | tee -a "$OUT"
echo "=== TEST 2: Proxy pass to Python backend ===" | tee -a "$OUT"
run_bench "Nproxy  /api/bench  ($CONNECTIONS conn, ${DURATION}s)" "$NPROXY/api/bench"
run_bench "Nginx   /api/bench  ($CONNECTIONS conn, ${DURATION}s)" "$NGINX/api/bench"

echo ""                                           | tee -a "$OUT"
echo "=== TEST 3: High concurrency (500 conn) ===" | tee -a "$OUT"
$WRK -t8 -c500 -d${DURATION}s --latency "$NPROXY/healthz" 2>&1 | tee -a "$OUT"
echo "---" | tee -a "$OUT"
$WRK -t8 -c500 -d${DURATION}s --latency "$NGINX/healthz"  2>&1 | tee -a "$OUT"

echo "" | tee -a "$OUT"
echo "Results saved to: $OUT"
cat "$OUT"
