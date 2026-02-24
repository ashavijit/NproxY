#ifndef NPROXY_TYPES_H
#define NPROXY_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef ssize_t  isize;
typedef size_t   usize;

typedef enum {
    NP_OK            =  0,
    NP_ERR           = -1,
    NP_ERR_AGAIN     = -2,
    NP_ERR_CLOSED    = -3,
    NP_ERR_NOMEM     = -4,
    NP_ERR_TIMEOUT   = -5,
    NP_ERR_PARSE     = -6,
    NP_ERR_CONFIG    = -7,
    NP_ERR_TLS       = -8,
    NP_ERR_UPSTREAM  = -9,
} np_status_t;

#define NP_MAX_HEADERS       64
#define NP_MAX_URI_LEN       8192
#define NP_MAX_HEADER_LEN    8192
#define NP_MAX_BACKENDS      64
#define NP_ARENA_SIZE        (64 * 1024)
#define NP_READ_BUF_SIZE     (64 * 1024)
#define NP_WRITE_BUF_SIZE    (128 * 1024)
#define NP_MAX_WORKERS       64
#define NP_EPOLL_EVENTS      1024
#define NP_TIMEOUT_BUCKETS   512

#ifndef LIKELY
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#define NP_UNUSED(x) ((void)(x))

#endif
