#ifndef NPROXY_DAEMON_H
#define NPROXY_DAEMON_H

#include "core/types.h"

np_status_t daemonize(const char *pid_file);
void pid_file_remove(const char *pid_file);
pid_t pid_file_read(const char *pid_file);

#endif
