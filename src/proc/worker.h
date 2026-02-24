#ifndef NPROXY_WORKER_H
#define NPROXY_WORKER_H

#include "core/config.h"
#include "net/socket.h"

int worker_run(np_config_t *cfg, np_socket_t *listener, int worker_id);

#endif
