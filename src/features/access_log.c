#include "features/access_log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_access_fd = -1;

void access_log_init(const char *path) {
  if (!path || path[0] == '\0') {
    g_access_fd = STDOUT_FILENO;
    return;
  }
  g_access_fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
}

void access_log_close(void) {
  if (g_access_fd >= 0 && g_access_fd != STDOUT_FILENO) {
    close(g_access_fd);
    g_access_fd = -1;
  }
}

void access_log_write(const http_request_t *req, int status, usize bytes,
                      const struct timespec *start) {
  if (g_access_fd < 0) return;

  struct timespec end;
  clock_gettime(CLOCK_MONOTONIC, &end);
  long latency_us =
      (long)((end.tv_sec - start->tv_sec) * 1000000L + (end.tv_nsec - start->tv_nsec) / 1000L);

  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  char tsbuf[32];
  strftime(tsbuf, sizeof(tsbuf), "%d/%b/%Y:%H:%M:%S +0000", &tm);

  char line[1024];
  int n = snprintf(line, sizeof(line), "%s - - [%s] \"%s " STR_FMT " HTTP/1.%d\" %d %zu %ldus\n",
                   req->remote_ip, tsbuf, http_method_str(req->method), STR_ARG(req->path),
                   req->version == HTTP_11 ? 1 : 0, status, bytes, latency_us);

  if (n > 0) {
    ssize_t _w = write(g_access_fd, line, (usize)n);
    NP_UNUSED(_w);
  }
}
