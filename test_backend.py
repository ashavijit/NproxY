from http.server import HTTPServer, BaseHTTPRequestHandler

class SimpleHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()
        response = f"Hello from Python! You requested: {self.path}"
        self.wfile.write(response.encode("utf-8"))

if __name__ == "__main__":
    HTTPServer.allow_reuse_address = True
    server = HTTPServer(("127.0.0.1", 8899), SimpleHandler)
    print("Python test server running on usually port 8899")
    server.serve_forever()
