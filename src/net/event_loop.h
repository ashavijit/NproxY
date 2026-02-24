#ifndef NPROXY_EVENT_LOOP_H
#define NPROXY_EVENT_LOOP_H

#include <sys/epoll.h>

#include "core/types.h"

#define EV_READ EPOLLIN
#define EV_WRITE EPOLLOUT
#define EV_EDGE EPOLLET
#define EV_HUP EPOLLRDHUP

typedef void (*ev_handler_fn)(int fd, u32 events, void *ctx);

typedef struct {
  ev_handler_fn fn;
  void *ctx;
} ev_handler_t;

typedef struct event_loop event_loop_t;

event_loop_t *event_loop_create(int max_events);
void event_loop_destroy(event_loop_t *loop);

np_status_t event_loop_add(event_loop_t *loop, int fd, u32 events, ev_handler_fn fn, void *ctx);
np_status_t event_loop_mod(event_loop_t *loop, int fd, u32 events, ev_handler_fn fn, void *ctx);
np_status_t event_loop_del(event_loop_t *loop, int fd);

void event_loop_run(event_loop_t *loop, int *running);

int event_loop_fd(const event_loop_t *loop);

#endif
