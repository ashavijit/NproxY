#ifndef NPROXY_STRING_UTIL_H
#define NPROXY_STRING_UTIL_H

#include <string.h>

#include "types.h"

typedef struct {
  const char *ptr;
  usize len;
} str_t;

#define STR(s) ((str_t){.ptr = (s), .len = sizeof(s) - 1})
#define STR_NULL ((str_t){.ptr = NULL, .len = 0})
#define STR_FMT "%.*s"
#define STR_ARG(s) (int)(s).len, (s).ptr

static inline str_t str_from_cstr(const char *s) {
  return (str_t){.ptr = s, .len = s ? strlen(s) : 0};
}

static inline bool str_eq(str_t a, str_t b) {
  return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

bool str_ieq(str_t a, str_t b);
str_t str_trim(str_t s);
str_t str_trim_left(str_t s);
str_t str_trim_right(str_t s);
bool str_starts_with(str_t s, str_t prefix);
bool str_ends_with(str_t s, str_t suffix);
str_t str_slice(str_t s, usize start, usize end);
int str_to_int(str_t s, i64 *out);

char *str_dup_cstr(const char *s, usize len);

int np_strncasecmp(const char *a, const char *b, usize n);

#endif
