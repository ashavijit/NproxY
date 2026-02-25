#include "proc/master.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/log.h"
#include "net/socket.h"
#include "proc/worker.h"

#define MAX_WORKERS 64

static pid_t worker_pids[MAX_WORKERS];
static int worker_count = 0;

static void spawn_worker(np_config_t *cfg, np_socket_t *listener, int id) {
  pid_t pid = fork();
  if (pid < 0) {
    log_error_errno("fork");
    return;
  }
  if (pid == 0) {
    int rc = worker_run(cfg, listener, id);
    exit(rc);
  }
  worker_pids[id] = pid;
  log_info("master: spawned worker[%d] pid=%d", id, (int)pid);
}

static volatile sig_atomic_t g_reload = 0;
static volatile sig_atomic_t g_shutdown = 0;

static void master_sighandler(int sig) {
  if (sig == SIGHUP)
    g_reload = 1;
  if (sig == SIGTERM || sig == SIGINT)
    g_shutdown = 1;
}

static void setup_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = master_sighandler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_DFL);
}

static void kill_workers(int sig) {
  for (int i = 0; i < worker_count; i++) {
    if (worker_pids[i] > 0)
      kill(worker_pids[i], sig);
  }
}

static void wait_workers(void) {
  for (int i = 0; i < worker_count; i++) {
    if (worker_pids[i] > 0) {
      waitpid(worker_pids[i], NULL, 0);
      worker_pids[i] = 0;
    }
  }
}

int master_run(np_config_t *cfg) {
  setup_signals();

  np_socket_t listener;
  if (socket_create_listener(&listener, cfg->listen_addr, cfg->listen_port, cfg->backlog) !=
      NP_OK) {
    log_error("master: failed to create listener");
    return 1;
  }

  worker_count = cfg->worker_processes;
  if (worker_count > MAX_WORKERS)
    worker_count = MAX_WORKERS;

  for (int i = 0; i < worker_count; i++) {
    spawn_worker(cfg, &listener, i);
  }

  log_info("master: pid=%d, %d workers running", (int)getpid(), worker_count);

  while (!g_shutdown) {
    int status;
    pid_t dead = waitpid(-1, &status, WNOHANG);
    if (dead > 0) {
      for (int i = 0; i < worker_count; i++) {
        if (worker_pids[i] == dead) {
          log_warn("master: worker[%d] pid=%d died, respawning", i, (int)dead);
          if (!g_shutdown)
            spawn_worker(cfg, &listener, i);
          break;
        }
      }
    }

    if (g_reload) {
      g_reload = 0;
      log_info("master: SIGHUP – graceful reload");
      kill_workers(SIGTERM);
      wait_workers();
      for (int i = 0; i < worker_count; i++) {
        spawn_worker(cfg, &listener, i);
      }
    }

    sleep(1);
  }

  log_info("master: shutting down");
  kill_workers(SIGTERM);
  wait_workers();
  socket_close(listener.fd);
  return 0;
}
