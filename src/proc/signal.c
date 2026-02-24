#include "proc/signal.h"
#include "core/log.h"
#include <signal.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>

typedef struct {
  int *running;
} sig_ctx_t;

static sig_ctx_t g_sig_ctx;

static void signal_handler(int fd, u32 events, void *arg) {
  NP_UNUSED(events);
  sig_ctx_t *ctx = (sig_ctx_t *)arg;
  struct signalfd_siginfo info;
  isize n = read(fd, &info, sizeof(info));
  if (n != sizeof(info))
    return;

  switch (info.ssi_signo) {
  case SIGTERM:
  case SIGINT:
    log_info("signal %d received, shutting down", info.ssi_signo);
    *ctx->running = 0;
    break;
  case SIGHUP:
    log_info("SIGHUP received, reload not implemented in worker context");
    break;
  default:
    break;
  }
}

np_status_t signal_init(event_loop_t *loop, int *running_flag) {
  g_sig_ctx.running = running_flag;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGPIPE);

  if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
    log_error_errno("sigprocmask");
    return NP_ERR;
  }

  int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
  if (sfd < 0) {
    log_error_errno("signalfd");
    return NP_ERR;
  }

  return event_loop_add(loop, sfd, EV_READ | EV_EDGE, signal_handler,
                        &g_sig_ctx);
}
