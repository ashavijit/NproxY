#ifndef NPROXY_BUFFER_H
#define NPROXY_BUFFER_H

#include "core/types.h"

typedef struct {
  u8 *data;
  usize cap;
  usize read_pos;
  usize write_pos;
} np_buf_t;

np_status_t buf_init(np_buf_t *b, usize cap);
void buf_free(np_buf_t *b);
void buf_reset(np_buf_t *b);

usize buf_readable(const np_buf_t *b);
usize buf_writable(const np_buf_t *b);

u8 *buf_read_ptr(const np_buf_t *b);
u8 *buf_write_ptr(np_buf_t *b);

void buf_consume(np_buf_t *b, usize n);
void buf_produce(np_buf_t *b, usize n);

isize buf_read_fd(np_buf_t *b, int fd);
isize buf_write_fd(np_buf_t *b, int fd);

void buf_compact(np_buf_t *b);

#endif
