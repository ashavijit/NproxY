#ifndef NPROXY_WORKER_H
#define NPROXY_WORKER_H

#include "core/config.h"
#include "net/conn.h"
#include "net/socket.h"

int worker_run(np_config_t *cfg, np_socket_t *listeners, int listener_count, int worker_id);

void worker_conn_close(conn_t *conn);
void worker_client_event_mod(conn_t *conn, u32 events);

#endif
