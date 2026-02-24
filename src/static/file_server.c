#include "static/file_server.h"
#include "core/log.h"
#include "core/string_util.h"
#include "http/response.h"
#include "static/mime.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

static bool path_is_safe(const char *path) {
  return strstr(path, "..") == NULL;
}

static const char *file_extension(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot || dot == path)
    return NULL;
  return dot + 1;
}

static void etag_from_stat(const struct stat *st, char *buf, usize bufsz) {
  snprintf(buf, bufsz, "\"%lx-%lx\"", (unsigned long)st->st_mtime,
           (unsigned long)st->st_size);
}

int file_server_handle(conn_t *conn, http_request_t *req, const char *root) {
  char resolved[8192];
  char path[4096];
  usize plen = req->path.len;
  if (plen >= sizeof(path))
    plen = sizeof(path) - 1;

  strncpy(path, req->path.ptr, plen);
  path[plen] = '\0';

  if (!path_is_safe(path)) {
    response_write_simple(&conn->wbuf, 403, "Forbidden", "text/plain",
                          "forbidden\n", req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    return 403;
  }

  if (path[plen - 1] == '/') {
    snprintf(resolved, sizeof(resolved), "%s%sindex.html", root, path);
  } else {
    snprintf(resolved, sizeof(resolved), "%s%s", root, path);
  }

  int fd = open(resolved, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    response_write_simple(&conn->wbuf, 404, "Not Found", "text/plain",
                          "not found\n", req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    return 404;
  }

  struct stat st;
  if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    response_write_simple(&conn->wbuf, 404, "Not Found", "text/plain",
                          "not found\n", req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    return 404;
  }

  char etag[64];
  etag_from_stat(&st, etag, sizeof(etag));

  str_t ims = request_header(req, STR("If-None-Match"));
  if (ims.len > 0) {
    char ims_buf[64];
    usize l = ims.len < sizeof(ims_buf) - 1 ? ims.len : sizeof(ims_buf) - 1;
    memcpy(ims_buf, ims.ptr, l);
    ims_buf[l] = '\0';
    if (strcmp(ims_buf, etag) == 0) {
      close(fd);
      response_write_simple(&conn->wbuf, 304, "Not Modified", NULL, NULL,
                            req->keep_alive);
      conn->state = CONN_WRITING_RESPONSE;
      return 304;
    }
  }

  const char *ext = file_extension(resolved);
  const char *mime = mime_by_extension(ext);

  char header[512];
  int hn = snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "ETag: %s\r\n"
                    "Connection: %s\r\n"
                    "\r\n",
                    mime, (long)st.st_size, etag,
                    req->keep_alive ? "keep-alive" : "close");

  if (hn > 0 && buf_writable(&conn->wbuf) >= (usize)hn) {
    memcpy(buf_write_ptr(&conn->wbuf), header, (usize)hn);
    buf_produce(&conn->wbuf, (usize)hn);
  }

  buf_write_fd(&conn->wbuf, conn->fd);

  off_t offset = 0;
  off_t remaining = st.st_size;
  while (remaining > 0) {
    ssize_t sent = sendfile(conn->fd, fd, &offset, (usize)remaining);
    if (sent < 0) {
      if (errno == EAGAIN)
        break;
      log_error_errno("sendfile fd=%d", conn->fd);
      break;
    }
    remaining -= sent;
  }

  close(fd);
  conn->state = req->keep_alive ? CONN_READING_REQUEST : CONN_CLOSING;
  return 200;
}
