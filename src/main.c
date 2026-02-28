#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/config.h"
#include "core/log.h"
#include "core/types.h"
#include "module/module.h"
#include "net/socket.h"
#include "proc/daemon.h"
#include "proc/master.h"
#include "proc/worker.h"

static void print_usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [-c config] [-t] [-w] [-d] [-v]\n"
          "  -c <file>   configuration file (default: nproxy.conf)\n"
          "  -t          test configuration and exit\n"
          "  -w          single worker mode (no fork, for development)\n"
          "  -d          run as daemon\n"
          "  -v          print version and exit\n",
          prog);
}

int main(int argc, char *argv[]) {
  const char *config_path = "nproxy.conf";
  bool test_only = false;
  bool single_worker = false;
  bool daemon_mode = false;
  int opt;

  while ((opt = getopt(argc, argv, "c:twdv")) != -1) {
    switch (opt) {
      case 'c':
        config_path = optarg;
        break;
      case 't':
        test_only = true;
        break;
      case 'w':
        single_worker = true;
        break;
      case 'd':
        daemon_mode = true;
        break;
      case 'v':
        fprintf(stdout, "nproxy 1.0.0\n");
        return 0;
      default:
        print_usage(argv[0]);
        return 1;
    }
  }

  np_config_t cfg;
  if (config_load(&cfg, config_path) != NP_OK) {
    fprintf(stderr, "nproxy: configuration error\n");
    return 1;
  }

  log_init(cfg.log.error_log, (log_level_t)cfg.log.level);

  if (module_load_all(&cfg) != 0) {
    fprintf(stderr, "nproxy: failed to load one or more modules\n");
    return 1;
  }

  if (test_only) {
    config_print(&cfg);
    fprintf(stdout, "configuration test successful\n");
    log_close();
    return 0;
  }

  config_print(&cfg);

  int rc;
  np_socket_t listeners[CONFIG_MAX_SERVERS];
  u16 bound_ports[CONFIG_MAX_SERVERS];
  int listener_count = 0;

  for (int i = 0; i < cfg.server_count; i++) {
    u16 port = cfg.servers[i].listen_port;
    bool already_bound = false;
    for (int j = 0; j < listener_count; j++) {
      if (bound_ports[j] == port) {
        already_bound = true;
        break;
      }
    }
    if (!already_bound) {
      if (socket_create_listener(&listeners[listener_count], cfg.listen_addr, port, cfg.backlog) !=
          NP_OK) {
        log_error("failed to bind %s:%d", cfg.listen_addr, port);
        log_close();
        return 1;
      }
      bound_ports[listener_count] = port;
      listener_count++;
    }
  }

  if (daemon_mode || cfg.process.daemon) {
    if (daemonize(cfg.process.pid_file) != NP_OK) {
      log_error("failed to daemonize");
      log_close();
      return 1;
    }
  }

  if (single_worker) {
    rc = worker_run(&cfg, listeners, listener_count, 0);
  } else {
    rc = master_run(&cfg, listeners, listener_count, config_path);
  }

  if (cfg.process.pid_file[0] != '\0')
    pid_file_remove(cfg.process.pid_file);

  module_unload_all();
  log_close();
  return rc;
}
