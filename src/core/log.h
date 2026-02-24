#ifndef NPROXY_LOG_H
#define NPROXY_LOG_H

#include <stdarg.h>

#include "types.h"

typedef enum {
  LOG_ERROR = 0,
  LOG_WARN = 1,
  LOG_INFO = 2,
  LOG_DEBUG = 3,
} log_level_t;

void log_init(const char *path, log_level_t level);
void log_close(void);
void log_write(log_level_t level, const char *fmt, ...);
void log_write_errno(log_level_t level, const char *fmt, ...);

#define log_error(...) log_write(LOG_ERROR, __VA_ARGS__)
#define log_warn(...) log_write(LOG_WARN, __VA_ARGS__)
#define log_info(...) log_write(LOG_INFO, __VA_ARGS__)
#define log_debug(...) log_write(LOG_DEBUG, __VA_ARGS__)
#define log_error_errno(...) log_write_errno(LOG_ERROR, __VA_ARGS__)

#endif
