#include "net/event_loop.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "core/log.h"

struct event_loop {
  int epfd;
  int max_events;
  struct epoll_event *events;
  ev_handler_t **handlers;
  int max_fd;
};

event_loop_t *event_loop_create(int max_events) {
  event_loop_t *loop = malloc(sizeof(*loop));
  if (!loop) return NULL;

  loop->epfd = epoll_create1(EPOLL_CLOEXEC);
  if (loop->epfd < 0) {
    log_error_errno("epoll_create1");
    free(loop);
    return NULL;
  }

  loop->max_events = max_events;
  loop->events = malloc(sizeof(struct epoll_event) * (usize)max_events);
  loop->max_fd = 65536;
  loop->handlers = calloc((usize)loop->max_fd, sizeof(ev_handler_t *));

  if (!loop->events || !loop->handlers) {
    close(loop->epfd);
    free(loop->events);
    free(loop->handlers);
    free(loop);
    return NULL;
  }
  return loop;
}

void event_loop_destroy(event_loop_t *loop) {
  if (!loop) return;
  for (int i = 0; i < loop->max_fd; i++) {
    free(loop->handlers[i]);
  }
  free(loop->handlers);
  free(loop->events);
  close(loop->epfd);
  free(loop);
}

static np_status_t loop_ctl(event_loop_t *loop, int op, int fd, u32 events, ev_handler_fn fn,
                            void *ctx) {
  if (UNLIKELY(fd >= loop->max_fd)) return NP_ERR;

  if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
    ev_handler_t *h = loop->handlers[fd];
    if (!h) {
      h = malloc(sizeof(ev_handler_t));
      if (!h) return NP_ERR_NOMEM;
      loop->handlers[fd] = h;
    }
    h->fn = fn;
    h->ctx = ctx;
  }

  struct epoll_event ev;
  ev.events = events;
  ev.data.ptr = loop->handlers[fd];

  if (epoll_ctl(loop->epfd, op, fd, &ev) < 0) {
    log_error_errno("epoll_ctl op=%d fd=%d", op, fd);
    return NP_ERR;
  }
  return NP_OK;
}

np_status_t event_loop_add(event_loop_t *loop, int fd, u32 events, ev_handler_fn fn, void *ctx) {
  return loop_ctl(loop, EPOLL_CTL_ADD, fd, events, fn, ctx);
}

np_status_t event_loop_mod(event_loop_t *loop, int fd, u32 events, ev_handler_fn fn, void *ctx) {
  return loop_ctl(loop, EPOLL_CTL_MOD, fd, events, fn, ctx);
}

np_status_t event_loop_del(event_loop_t *loop, int fd) {
  if (fd >= loop->max_fd) return NP_ERR;
  if (epoll_ctl(loop->epfd, EPOLL_CTL_DEL, fd, NULL) < 0 && errno != EBADF) {
    log_error_errno("epoll_ctl EPOLL_CTL_DEL fd=%d", fd);
  }
  free(loop->handlers[fd]);
  loop->handlers[fd] = NULL;
  return NP_OK;
}

void event_loop_run(event_loop_t *loop, int *running) {
  while (*running) {
    int n = epoll_wait(loop->epfd, loop->events, loop->max_events, 1000);
    if (n < 0) {
      if (errno == EINTR) continue;
      log_error_errno("epoll_wait");
      break;
    }
    for (int i = 0; i < n; i++) {
      ev_handler_t *h = (ev_handler_t *)loop->events[i].data.ptr;
      u32 ev = loop->events[i].events;
      if (LIKELY(h)) {
        h->fn(0, ev, h->ctx);
      }
    }
  }
}

int event_loop_fd(const event_loop_t *loop) {
  return loop->epfd;
}
