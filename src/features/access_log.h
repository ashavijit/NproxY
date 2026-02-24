#ifndef NPROXY_ACCESS_LOG_H
#define NPROXY_ACCESS_LOG_H

#include <time.h>

#include "http/request.h"

void access_log_init(const char *path);
void access_log_close(void);
void access_log_write(const http_request_t *req, int status, usize bytes,
                      const struct timespec *start);

#endif
