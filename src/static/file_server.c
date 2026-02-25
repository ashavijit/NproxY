#include "static/file_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/log.h"
#include "core/string_util.h"
#include "http/response.h"
#include "net/event_loop.h"
#include "proc/worker.h"
#include "static/mime.h"

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
  snprintf(buf, bufsz, "\"%lx-%lx\"", (unsigned long)st->st_mtime, (unsigned long)st->st_size);
}

int file_server_handle(conn_t *conn, http_request_t *req, np_config_t *cfg) {
  char resolved[8192];
  char path[4096];
  usize plen = req->path.len;
  if (plen >= sizeof(path))
    plen = sizeof(path) - 1;

  strncpy(path, req->path.ptr, plen);
  path[plen] = '\0';

  if (!path_is_safe(path)) {
    response_write_error(&conn->wbuf, 403, req->keep_alive);
    conn->state = CONN_WRITING_RESPONSE;
    return 403;
  }

  int fd = -1;
  struct stat st;

  if (cfg->try_files.count > 0) {
    for (int i = 0; i < cfg->try_files.count; i++) {
      char tmp_path[8192] = {0};
      char *src = cfg->try_files.paths[i];
      char *tgt = tmp_path;
      while (*src && (tgt - tmp_path) < (isize)sizeof(tmp_path) - 1) {
        if (strncmp(src, "$uri", 4) == 0) {
          int ulen = snprintf(tgt, sizeof(tmp_path) - (tgt - tmp_path), "%s", path);
          if (ulen > 0)
            tgt += ulen;
          src += 4;
        } else {
          *tgt++ = *src++;
        }
      }
      *tgt = '\0';

      snprintf(resolved, sizeof(resolved), "%s%s", cfg->static_root, tmp_path);
      fd = open(resolved, O_RDONLY | O_CLOEXEC);
      if (fd >= 0) {
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
          break;  // found a matching file
        }
        close(fd);
        fd = -1;
      }
    }
  } else {
    if (path[plen - 1] == '/') {
      snprintf(resolved, sizeof(resolved), "%s%sindex.html", cfg->static_root, path);
    } else {
      snprintf(resolved, sizeof(resolved), "%s%s", cfg->static_root, path);
    }
    fd = open(resolved, O_RDONLY | O_CLOEXEC);
    if (fd >= 0 && (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode))) {
      close(fd);
      fd = -1;
    }
  }

  if (fd < 0) {
    response_write_error(&conn->wbuf, 404, req->keep_alive);
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
      response_write_simple(&conn->wbuf, 304, "Not Modified", NULL, NULL, req->keep_alive);
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
                    mime, (long)st.st_size, etag, req->keep_alive ? "keep-alive" : "close");

  if (hn > 0 && buf_writable(&conn->wbuf) >= (usize)hn) {
    memcpy(buf_write_ptr(&conn->wbuf), header, (usize)hn);
    buf_produce(&conn->wbuf, (usize)hn);
  }

  buf_write_fd(&conn->wbuf, conn->fd);

  conn->file_fd = fd;
  conn->file_offset = 0;
  conn->file_remaining = st.st_size;
  conn->state = CONN_SENDFILE;

  worker_client_event_mod(conn, EV_WRITE | EV_READ | EV_HUP | EV_EDGE);

  return 200;
}
