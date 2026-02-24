#ifndef NPROXY_ACCESS_LOG_H
#define NPROXY_ACCESS_LOG_H

#include "http/request.h"
#include <time.h>

void access_log_init(const char *path);
void access_log_close(void);
void access_log_write(const http_request_t *req, int status, usize bytes,
                      const struct timespec *start);

#endif
