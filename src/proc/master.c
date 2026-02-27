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

static void spawn_worker(np_config_t *cfg, np_socket_t *listeners, int listener_count, int id) {
  pid_t pid = fork();
  if (pid < 0) {
    log_error_errno("fork");
    return;
  }
  if (pid == 0) {
    int rc = worker_run(cfg, listeners, listener_count, id);
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

int master_run(np_config_t *cfg, np_socket_t *listeners, int listener_count,
               const char *config_path) {
  setup_signals();

  worker_count = cfg->worker_processes;
  if (worker_count > MAX_WORKERS)
    worker_count = MAX_WORKERS;

  for (int i = 0; i < worker_count; i++) {
    spawn_worker(cfg, listeners, listener_count, i);
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
            spawn_worker(cfg, listeners, listener_count, i);
          break;
        }
      }
    }

    if (g_reload) {
      g_reload = 0;
      log_info("master: SIGHUP - reloading configuration");

      np_config_t new_cfg;
      if (config_load(&new_cfg, config_path) == NP_OK) {
        kill_workers(SIGTERM);
        wait_workers();

        for (int i = 0; i < listener_count; i++) {
          socket_close(listeners[i].fd);
        }

        *cfg = new_cfg;
        listener_count = 0;

        u16 bound_ports[CONFIG_MAX_SERVERS];
        for (int i = 0; i < cfg->server_count; i++) {
          u16 port = cfg->servers[i].listen_port;
          bool already = false;
          for (int j = 0; j < listener_count; j++) {
            if (bound_ports[j] == port) {
              already = true;
              break;
            }
          }
          if (!already) {
            if (socket_create_listener(&listeners[listener_count], cfg->listen_addr, port,
                                       cfg->backlog) == NP_OK) {
              bound_ports[listener_count] = port;
              listener_count++;
            } else {
              log_error("master: reload failed to bind %s:%d", cfg->listen_addr, port);
            }
          }
        }

        worker_count = cfg->worker_processes;
        if (worker_count > MAX_WORKERS)
          worker_count = MAX_WORKERS;

        for (int i = 0; i < worker_count; i++) {
          spawn_worker(cfg, listeners, listener_count, i);
        }

        log_info("master: reload complete, %d workers running", worker_count);
      } else {
        log_error("master: reload failed, keeping current configuration");
      }
    }

    sleep(1);
  }

  log_info("master: shutting down");
  kill_workers(SIGTERM);
  wait_workers();
  for (int i = 0; i < listener_count; i++) {
    socket_close(listeners[i].fd);
  }
  return 0;
}
