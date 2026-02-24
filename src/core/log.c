#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_log_fd = STDERR_FILENO;
static log_level_t g_log_level = LOG_INFO;

static const char *level_str[] = {
    [LOG_ERROR] = "ERROR",
    [LOG_WARN] = "WARN ",
    [LOG_INFO] = "INFO ",
    [LOG_DEBUG] = "DEBUG",
};

void log_init(const char *path, log_level_t level) {
  g_log_level = level;
  if (!path || path[0] == '\0')
    return;
  int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd < 0) {
    fprintf(stderr, "log_init: cannot open %s: %s\n", path, strerror(errno));
    return;
  }
  g_log_fd = fd;
}

void log_close(void) {
  if (g_log_fd != STDERR_FILENO) {
    close(g_log_fd);
    g_log_fd = STDERR_FILENO;
  }
}

static void log_vwrite(log_level_t level, const char *fmt, va_list ap,
                       int use_errno) {
  if (level > g_log_level)
    return;

  char buf[4096];
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm;
  gmtime_r(&ts.tv_sec, &tm);

  int hdr = snprintf(
      buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ [%s] [%d] ",
      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
      tm.tm_sec, ts.tv_nsec / 1000000L, level_str[level], (int)getpid());

  if (hdr < 0)
    hdr = 0;

  int body = vsnprintf(buf + hdr, sizeof(buf) - (usize)hdr, fmt, ap);
  int total = hdr + (body > 0 ? body : 0);

  if (use_errno && total < (int)sizeof(buf) - 64) {
    total += snprintf(buf + total, sizeof(buf) - (usize)total, ": %s",
                      strerror(errno));
  }

  if (total < (int)sizeof(buf) - 1) {
    buf[total++] = '\n';
  }
  buf[total] = '\0';

  ssize_t _wr = write(g_log_fd, buf, (usize)total);
  NP_UNUSED(_wr);
}

void log_write(log_level_t level, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_vwrite(level, fmt, ap, 0);
  va_end(ap);
}

void log_write_errno(log_level_t level, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  log_vwrite(level, fmt, ap, 1);
  va_end(ap);
}
