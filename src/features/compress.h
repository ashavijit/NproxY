#ifndef NPROXY_COMPRESS_H
#define NPROXY_COMPRESS_H

#include "core/types.h"
#include "http/request.h"

bool client_accepts_gzip(http_request_t *req);
bool should_compress(const char *content_type);
np_status_t gzip_compress(const u8 *in, usize in_len, u8 *out, usize out_cap, usize *out_len);

#endif
