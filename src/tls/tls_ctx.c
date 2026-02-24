#include "tls/tls_ctx.h"
#include "core/log.h"
#include <openssl/err.h>
#include <openssl/ssl.h>

static int sni_callback(SSL *ssl, int *al, void *arg) {
  NP_UNUSED(al);
  NP_UNUSED(arg);
  NP_UNUSED(ssl);
  return SSL_TLSEXT_ERR_OK;
}

np_status_t tls_ctx_create(np_tls_ctx_t *tc, const np_config_t *cfg) {
  OPENSSL_init_ssl(
      OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);

  const SSL_METHOD *method = TLS_server_method();
  tc->ctx = SSL_CTX_new(method);
  if (!tc->ctx) {
    log_error("tls_ctx_create: SSL_CTX_new failed");
    return NP_ERR_TLS;
  }

  SSL_CTX_set_min_proto_version(tc->ctx, TLS1_2_VERSION);
  SSL_CTX_set_options(tc->ctx, SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                                   SSL_OP_NO_TLSv1_1 | SSL_OP_SINGLE_DH_USE |
                                   SSL_OP_CIPHER_SERVER_PREFERENCE);

  SSL_CTX_set_session_cache_mode(tc->ctx, SSL_SESS_CACHE_SERVER);
  SSL_CTX_sess_set_cache_size(tc->ctx, 4096);

  SSL_CTX_set_tlsext_servername_callback(tc->ctx, sni_callback);

  if (SSL_CTX_use_certificate_file(tc->ctx, cfg->tls.cert_file,
                                   SSL_FILETYPE_PEM) <= 0) {
    log_error("tls_ctx_create: failed to load cert %s", cfg->tls.cert_file);
    SSL_CTX_free(tc->ctx);
    tc->ctx = NULL;
    return NP_ERR_TLS;
  }

  if (SSL_CTX_use_PrivateKey_file(tc->ctx, cfg->tls.key_file,
                                  SSL_FILETYPE_PEM) <= 0) {
    log_error("tls_ctx_create: failed to load key %s", cfg->tls.key_file);
    SSL_CTX_free(tc->ctx);
    tc->ctx = NULL;
    return NP_ERR_TLS;
  }

  if (!SSL_CTX_check_private_key(tc->ctx)) {
    log_error("tls_ctx_create: cert/key mismatch");
    SSL_CTX_free(tc->ctx);
    tc->ctx = NULL;
    return NP_ERR_TLS;
  }

  log_info("TLS context initialized: cert=%s", cfg->tls.cert_file);
  return NP_OK;
}

void tls_ctx_destroy(np_tls_ctx_t *tc) {
  if (tc->ctx) {
    SSL_CTX_free(tc->ctx);
    tc->ctx = NULL;
  }
}

SSL_CTX *tls_ctx_get(const np_tls_ctx_t *tc) { return tc->ctx; }
