#include "features/compress.h"

#include <string.h>
#include <zlib.h>

#include "core/string_util.h"

bool client_accepts_gzip(http_request_t *req) {
  str_t ae = request_header(req, STR("Accept-Encoding"));
  if (!ae.ptr || ae.len == 0)
    return false;
  char buf[512];
  usize len = ae.len < sizeof(buf) - 1 ? ae.len : sizeof(buf) - 1;
  memcpy(buf, ae.ptr, len);
  buf[len] = '\0';
  return strstr(buf, "gzip") != NULL;
}

bool should_compress(const char *content_type) {
  if (!content_type)
    return false;
  if (strncmp(content_type, "text/", 5) == 0)
    return true;
  if (strstr(content_type, "json"))
    return true;
  if (strstr(content_type, "javascript"))
    return true;
  if (strstr(content_type, "xml"))
    return true;
  if (strstr(content_type, "svg"))
    return true;
  return false;
}

np_status_t gzip_compress(const u8 *in, usize in_len, u8 *out, usize out_cap, usize *out_len) {
  z_stream zs;
  memset(&zs, 0, sizeof(zs));

  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) !=
      Z_OK)
    return NP_ERR;

  zs.next_in = (Bytef *)in;
  zs.avail_in = (uInt)in_len;
  zs.next_out = (Bytef *)out;
  zs.avail_out = (uInt)out_cap;

  int ret = deflate(&zs, Z_FINISH);
  deflateEnd(&zs);

  if (ret != Z_STREAM_END)
    return NP_ERR;

  *out_len = (usize)zs.total_out;
  return NP_OK;
}
