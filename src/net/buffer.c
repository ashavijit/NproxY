#include "net/buffer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

np_status_t buf_init(np_buf_t *b, usize cap) {
  b->data = malloc(cap);
  if (!b->data)
    return NP_ERR_NOMEM;
  b->cap = cap;
  b->read_pos = 0;
  b->write_pos = 0;
  return NP_OK;
}

void buf_free(np_buf_t *b) {
  free(b->data);
  b->data = NULL;
  b->cap = 0;
  b->read_pos = 0;
  b->write_pos = 0;
}

void buf_reset(np_buf_t *b) {
  b->read_pos = 0;
  b->write_pos = 0;
}

usize buf_readable(const np_buf_t *b) {
  return b->write_pos - b->read_pos;
}

usize buf_writable(const np_buf_t *b) {
  return b->cap - b->write_pos;
}

u8 *buf_read_ptr(const np_buf_t *b) {
  return b->data + b->read_pos;
}

u8 *buf_write_ptr(np_buf_t *b) {
  return b->data + b->write_pos;
}

void buf_consume(np_buf_t *b, usize n) {
  b->read_pos += n;
  if (b->read_pos == b->write_pos) {
    b->read_pos = 0;
    b->write_pos = 0;
  }
}

void buf_produce(np_buf_t *b, usize n) {
  b->write_pos += n;
}

void buf_compact(np_buf_t *b) {
  usize readable = buf_readable(b);
  if (readable == 0) {
    buf_reset(b);
    return;
  }
  if (b->read_pos > 0) {
    memmove(b->data, b->data + b->read_pos, readable);
    b->read_pos = 0;
    b->write_pos = readable;
  }
}

isize buf_read_fd(np_buf_t *b, int fd) {
  buf_compact(b);
  usize space = buf_writable(b);
  if (space == 0)
    return 0;

  isize n = read(fd, buf_write_ptr(b), space);
  if (n > 0) {
    buf_produce(b, (usize)n);
  } else if (n == 0) {
    return NP_ERR_CLOSED;
  } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return NP_ERR_AGAIN;
  } else {
    return NP_ERR;
  }
  return n;
}

isize buf_write_fd(np_buf_t *b, int fd) {
  usize readable = buf_readable(b);
  if (readable == 0)
    return 0;

  isize n = write(fd, buf_read_ptr(b), readable);
  if (n > 0) {
    buf_consume(b, (usize)n);
  } else if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return NP_ERR_AGAIN;
    return NP_ERR;
  }
  return n;
}
