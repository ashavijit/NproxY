#ifndef NPROXY_MASTER_H
#define NPROXY_MASTER_H

#include "core/config.h"
#include "net/socket.h"

int master_run(np_config_t *cfg, np_socket_t *listeners, int listener_count);

#endif
