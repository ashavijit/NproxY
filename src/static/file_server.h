#ifndef NPROXY_FILE_SERVER_H
#define NPROXY_FILE_SERVER_H

#include "core/config.h"
#include "core/types.h"
#include "http/request.h"
#include "net/conn.h"

int file_server_handle(conn_t *conn, http_request_t *req, np_config_t *cfg);

#endif
