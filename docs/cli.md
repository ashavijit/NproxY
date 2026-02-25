# CLI Reference

Nproxy is controlled entirely through command-line flags and a configuration file.

---

## Usage

```
nproxy [-c config] [-t] [-w] [-v]
```

---

## Flags

| Flag | Argument | Default | Description |
|---|---|---|---|
| `-c` | `<file>` | `nproxy.conf` | Path to configuration file |
| `-t` | *(none)* | -- | Test configuration and exit. Parses the config, prints a summary, and exits with code 0 on success or 1 on error |
| `-w` | *(none)* | -- | Single worker mode. Runs in the foreground without forking. Useful for development and debugging |
| `-v` | *(none)* | -- | Print version (`nproxy 1.0.0`) and exit |

---

## Examples

```bash
# Start with default config (nproxy.conf in current directory)
./nproxy

# Start with a custom config file
./nproxy -c /etc/nproxy/nproxy.conf

# Validate configuration without starting
./nproxy -t
./nproxy -t -c /etc/nproxy/nproxy.conf

# Development mode (single worker, foreground, easy to Ctrl+C)
./nproxy -w

# Print version
./nproxy -v
# Output: nproxy 1.0.0
```

---

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Success (normal exit, or `-t` / `-v` completed) |
| `1` | Error (config parse failure, bind failure, etc.) |

---

## Signals

Once running, Nproxy responds to these signals:

| Signal | Action |
|---|---|
| `SIGTERM` | Graceful shutdown -- workers finish in-flight requests, then exit |
| `SIGINT` | Immediate shutdown (Ctrl+C) |
| `SIGHUP` | Graceful reload -- workers are replaced with new ones using the current config |
| `SIGPIPE` | Ignored (prevents crashes on broken pipe writes) |

### Signal Examples

```bash
# Graceful config reload
kill -HUP $(pidof nproxy)

# Graceful shutdown
kill -TERM $(pidof nproxy)

# Using systemd
sudo systemctl reload nproxy    # sends SIGHUP
sudo systemctl stop nproxy      # sends SIGTERM
```

---

## Process Model

| Mode | Processes | Use Case |
|---|---|---|
| Default (`./nproxy`) | 1 master + N workers | Production |
| Single worker (`./nproxy -w`) | 1 process (no fork) | Development, debugging, containers |

In single-worker mode, the process runs in the foreground and handles all connections in a single event loop. This is ideal for development, debugging with AddressSanitizer, or running inside containers where you want a single process.
