#include "tls/tls_conn.h"

#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdlib.h>

#include "core/log.h"

np_tls_conn_t *tls_conn_create(SSL_CTX *ctx, int fd) {
  np_tls_conn_t *tc = malloc(sizeof(*tc));
  if (!tc)
    return NULL;
  tc->ssl = SSL_new(ctx);
  if (!tc->ssl) {
    free(tc);
    return NULL;
  }
  SSL_set_fd(tc->ssl, fd);
  SSL_set_accept_state(tc->ssl);
  tc->handshake_done = false;
  return tc;
}

void tls_conn_destroy(np_tls_conn_t *tc) {
  if (!tc)
    return;
  if (tc->ssl) {
    SSL_shutdown(tc->ssl);
    SSL_free(tc->ssl);
  }
  free(tc);
}

np_status_t tls_conn_handshake(np_tls_conn_t *tc) {
  if (tc->handshake_done)
    return NP_OK;
  int rc = SSL_do_handshake(tc->ssl);
  if (rc == 1) {
    tc->handshake_done = true;
    return NP_OK;
  }
  int err = SSL_get_error(tc->ssl, rc);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    return NP_ERR_AGAIN;
  }
  log_error("TLS handshake failed: err=%d", err);
  return NP_ERR_TLS;
}

isize tls_conn_read(np_tls_conn_t *tc, u8 *buf, usize len) {
  int n = SSL_read(tc->ssl, buf, (int)len);
  if (n > 0)
    return (isize)n;
  int err = SSL_get_error(tc->ssl, n);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    return NP_ERR_AGAIN;
  if (err == SSL_ERROR_ZERO_RETURN)
    return NP_ERR_CLOSED;
  return NP_ERR;
}

isize tls_conn_write(np_tls_conn_t *tc, const u8 *buf, usize len) {
  int n = SSL_write(tc->ssl, buf, (int)len);
  if (n > 0)
    return (isize)n;
  int err = SSL_get_error(tc->ssl, n);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    return NP_ERR_AGAIN;
  return NP_ERR;
}
