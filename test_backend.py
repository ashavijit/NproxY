#!/usr/bin/env python3
"""Simple HTTP backend server for Nproxy integration testing.
Usage: python3 test_backend.py <port>
"""
import sys
import json
import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 9000

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[backend:{PORT}] {fmt % args}", flush=True)

    def do_GET(self):
        body = json.dumps({
            "backend_port": PORT,
            "path": self.path,
            "method": "GET",
            "time": datetime.datetime.utcnow().isoformat() + "Z",
            "headers": dict(self.headers),
        }, indent=2).encode()

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Backend-Port", str(PORT))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        payload = self.rfile.read(length)
        body = json.dumps({
            "backend_port": PORT,
            "path": self.path,
            "method": "POST",
            "body_bytes": len(payload),
        }).encode()

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

print(f"[backend] Listening on port {PORT}", flush=True)
HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
