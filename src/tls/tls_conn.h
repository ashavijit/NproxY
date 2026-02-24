#ifndef NPROXY_TLS_CONN_H
#define NPROXY_TLS_CONN_H

#include <openssl/ssl.h>

#include "core/types.h"
#include "net/conn.h"

typedef struct {
  SSL *ssl;
  bool handshake_done;
} np_tls_conn_t;

np_tls_conn_t *tls_conn_create(SSL_CTX *ctx, int fd);
void tls_conn_destroy(np_tls_conn_t *tc);
np_status_t tls_conn_handshake(np_tls_conn_t *tc);
isize tls_conn_read(np_tls_conn_t *tc, u8 *buf, usize len);
isize tls_conn_write(np_tls_conn_t *tc, const u8 *buf, usize len);

#endif
