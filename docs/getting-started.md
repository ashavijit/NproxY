# Getting Started

This guide walks you through installing Nproxy, running it for the first time, and proxying traffic to a backend.

---

## Prerequisites

| Requirement | Minimum Version |
|---|---|
| GCC or Clang | GCC 10+ / Clang 12+ |
| GNU Make | 3.81+ |
| OpenSSL development headers | 1.1+ |
| Linux kernel | 2.6.27+ (for `epoll`, `signalfd`, `sendfile`) |

Install build dependencies:

```bash
# Ubuntu / Debian
sudo apt install build-essential libssl-dev

# Fedora / RHEL / CentOS
sudo dnf install gcc make openssl-devel

# Arch Linux
sudo pacman -S base-devel openssl
```

---

## Installation

### Option A: One-line install (system-wide)

```bash
curl -fsSL https://raw.githubusercontent.com/ashavijit/NproxY/main/install.sh | bash
```

This compiles from source, installs the binary to `/usr/local/bin/nproxy`, creates the config at `/etc/nproxy/nproxy.conf`, and sets up log/run directories.

### Option B: Local install (no root)

```bash
git clone https://github.com/ashavijit/NproxY.git
cd NproxY
bash install.sh --local
```

The binary is placed at `./bin/nproxy`. Add it to your PATH:

```bash
export PATH="$PATH:$(pwd)/bin"
```

### Option C: Build manually

```bash
git clone https://github.com/ashavijit/NproxY.git
cd NproxY
make            # optimized build (~70 KB binary)
```

The `nproxy` binary appears in the project root.

Other build targets:

| Target | Description |
|---|---|
| `make` | Optimized release build (`-O2 -march=native`) |
| `make debug` | Debug build with AddressSanitizer (`-g -O0 -fsanitize=address,undefined`) |
| `make clean` | Remove all build artifacts |
| `make format` | Auto-format source with `clang-format` |

---

## First Run

### 1. Create a configuration file

```bash
cp nproxy.conf.example nproxy.conf
```

### 2. Validate the config

```bash
./nproxy -t
```

If everything is correct, you'll see:

```
configuration test successful
```

### 3. Start Nproxy

**Development mode** (single worker, no fork -- easy to debug):

```bash
./nproxy -w
```

**Production mode** (multi-worker, forked):

```bash
./nproxy
```

### 4. Verify it's running

```bash
curl http://localhost:8080/healthz
# {"status":"ok"}
```

---

## Quick Example: Reverse Proxy

Proxy all traffic to two backend servers on ports 9000 and 9001:

**nproxy.conf:**

```ini
[server]
listen_port = 8080

[proxy]
enabled = true
mode = round_robin

[upstream]
backend = 127.0.0.1:9000
backend = 127.0.0.1:9001
```

Start test backends (for example, Python's built-in HTTP server):

```bash
mkdir -p /tmp/backend && echo "Hello from backend" > /tmp/backend/index.html
cd /tmp/backend
python3 -m http.server 9000 &
python3 -m http.server 9001 &
```

Start Nproxy and send a request:

```bash
./nproxy -w &
curl -v http://localhost:8080/
```

---

## Quick Example: Static File Server

Serve files from a local directory:

**nproxy.conf:**

```ini
[server]
listen_port = 8080
static_root = ./www

[proxy]
enabled = false
```

Create the `www/` directory and add content:

```bash
mkdir -p www
echo "<h1>Hello, Nproxy!</h1>" > www/index.html
```

```bash
./nproxy -w &
curl http://localhost:8080/
# <h1>Hello, Nproxy!</h1>
```

---

## Graceful Reload

Reload configuration without dropping connections:

```bash
kill -HUP $(pidof nproxy)
```

---

## Stopping Nproxy

```bash
# Graceful shutdown (finishes in-flight requests)
kill -TERM $(pidof nproxy)

# Immediate shutdown
kill -INT $(pidof nproxy)
```

---

## What's Next

- [Configuration Reference](configuration.md) -- full directive listing
- [Architecture](architecture.md) -- understand how Nproxy works internally
- [Deployment](deployment.md) -- run Nproxy in production with systemd
