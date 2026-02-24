#ifndef NPROXY_CONFIG_H
#define NPROXY_CONFIG_H

#include "types.h"

#define CONFIG_MAX_BACKENDS 64
#define CONFIG_MAX_STR 512

typedef enum {
  BALANCE_ROUND_ROBIN = 0,
  BALANCE_LEAST_CONN = 1,
} balance_mode_t;

typedef struct {
  char host[256];
  u16 port;
  bool enabled;
} backend_entry_t;

typedef struct {
  u16 listen_port;
  char listen_addr[64];
  int worker_processes;
  int backlog;
  int max_connections;
  int keepalive_timeout;
  int read_timeout;
  int write_timeout;
  char static_root[CONFIG_MAX_STR];

  struct {
    bool enabled;
    u16 listen_port;
    char cert_file[CONFIG_MAX_STR];
    char key_file[CONFIG_MAX_STR];
  } tls;

  struct {
    bool enabled;
    balance_mode_t mode;
    int connect_timeout;
    int upstream_timeout;
    backend_entry_t backends[CONFIG_MAX_BACKENDS];
    int backend_count;
  } proxy;

  struct {
    bool enabled;
    int requests_per_second;
    int burst;
  } rate_limit;

  struct {
    int level;
    char access_log[CONFIG_MAX_STR];
    char error_log[CONFIG_MAX_STR];
  } log;

  struct {
    bool enabled;
    char path[CONFIG_MAX_STR];
  } metrics;
} np_config_t;

np_status_t config_load(np_config_t *cfg, const char *path);
void config_destroy(np_config_t *cfg);
void config_print(const np_config_t *cfg);

#endif
