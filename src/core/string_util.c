#include "string_util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

int np_strncasecmp(const char *a, const char *b, usize n) {
  for (usize i = 0; i < n; i++) {
    int ca = tolower((unsigned char)a[i]);
    int cb = tolower((unsigned char)b[i]);
    if (ca != cb)
      return ca - cb;
    if (ca == 0)
      return 0;
  }
  return 0;
}

bool str_ieq(str_t a, str_t b) {
  if (a.len != b.len)
    return false;
  return np_strncasecmp(a.ptr, b.ptr, a.len) == 0;
}

str_t str_trim_left(str_t s) {
  while (s.len > 0 && isspace((unsigned char)*s.ptr)) {
    s.ptr++;
    s.len--;
  }
  return s;
}

str_t str_trim_right(str_t s) {
  while (s.len > 0 && isspace((unsigned char)s.ptr[s.len - 1])) {
    s.len--;
  }
  return s;
}

str_t str_trim(str_t s) { return str_trim_right(str_trim_left(s)); }

bool str_starts_with(str_t s, str_t prefix) {
  if (s.len < prefix.len)
    return false;
  return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

bool str_ends_with(str_t s, str_t suffix) {
  if (s.len < suffix.len)
    return false;
  return memcmp(s.ptr + s.len - suffix.len, suffix.ptr, suffix.len) == 0;
}

str_t str_slice(str_t s, usize start, usize end) {
  if (start > s.len)
    start = s.len;
  if (end > s.len)
    end = s.len;
  if (start > end)
    start = end;
  return (str_t){.ptr = s.ptr + start, .len = end - start};
}

int str_to_int(str_t s, i64 *out) {
  if (s.len == 0)
    return -1;
  char buf[32];
  if (s.len >= sizeof(buf))
    return -1;
  memcpy(buf, s.ptr, s.len);
  buf[s.len] = '\0';
  char *end;
  long long v = strtoll(buf, &end, 10);
  if (end == buf || *end != '\0')
    return -1;
  *out = (i64)v;
  return 0;
}

char *str_dup_cstr(const char *s, usize len) {
  char *buf = malloc(len + 1);
  if (!buf)
    return NULL;
  memcpy(buf, s, len);
  buf[len] = '\0';
  return buf;
}
