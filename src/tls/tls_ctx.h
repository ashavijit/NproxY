#ifndef NPROXY_TLS_CTX_H
#define NPROXY_TLS_CTX_H

#include <openssl/ssl.h>

#include "core/config.h"
#include "core/types.h"

typedef struct {
  SSL_CTX *ctx;
} np_tls_ctx_t;

np_status_t tls_ctx_create(np_tls_ctx_t *tc, const np_config_t *cfg);
void tls_ctx_destroy(np_tls_ctx_t *tc);
SSL_CTX *tls_ctx_get(const np_tls_ctx_t *tc);

#endif
