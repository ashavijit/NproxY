# Nproxy Future Architectural Steps

This document outlines potential future features and architectural optimizations for the Nproxy codebase, building upon its current high-performance foundation (epoll, dynamic modules, robust routing).

## 1. Advanced Web Protocol Support
- **HTTP/2 & HTTP/3 (QUIC)**: Nproxy currently parses HTTP/1.1. Implementing HTTP/2 multiplexing (HPACK, stream frames) would provide a massive performance upgrade. HTTP/3 via QUIC over UDP represents the next modern tier of web serving.
- **WebSockets (`Upgrade` header)**: Add a mechanism to detect `Connection: Upgrade` and bi-directionally stream raw TCP, enabling WebSocket tunnels without dropping connections.

## 2. Upstream Proxy Enhancements
- **Keep-Alive Upstream Connections**: Currently, Nproxy aggressively closes upstream proxy connections after serving a request (HTTP/1.0 style). Implementing an upstream connection pool that keeps backend sockets open (HTTP/1.1 `keep-alive`) would drastically reduce latency and TCP handshake overhead.
- **Active Health Checks**: Transition from passive checks (counting 502s) to proactive HTTP/TCP pings on a timer. This allows Nproxy to smoothly remove dead nodes *before* routing user traffic to them.
- **Advanced Load Balancing Algorithms**: Expand beyond Round-Robin and Least-Connections to include IP Hashing (for sticky sessions) and Weighted Round-Robin (for disproportionally sized backend clusters).

## 3. Caching and Buffer Optimizations
- **Static File `sendfile(2)`**: Optimize static file delivery by utilizing Linux's zero-copy `sendfile(2)` syscall. This blasts file responses directly from the filesystem to the network socket, entirely bypassing user-space memory operations.
- **Proxy Response Caching**: Introduce a memory or disk-backed cache for upstream responses (parsing `Cache-Control` and `Vary` headers) to significantly decrease backend load for frequently accessed endpoints.

## 4. Configuration and Usability
- **Hot Configuration Reloading**: Upgrade the `SIGHUP` reload behavior in `master.c`. True seamless reloading requires the master to parse the new config, spin up *new* workers with the new config, wait for old workers to drain their existing connections gracefully, and shut down the old workers without dropping a single packet.
- **Virtual Hosts / Server Blocks**: The `np_config_t` currently assumes a single `[server]` block per instance. Implement a configuration hierarchy to parse multiple `server_name` blocks (SNI/Host header multiplexing) to host multiple domains dynamically on the same Nproxy port.
