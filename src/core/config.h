#ifndef NPROXY_CONFIG_H
#define NPROXY_CONFIG_H

#include <regex.h>

#include "types.h"

#define CONFIG_MAX_BACKENDS 64
#define CONFIG_MAX_STR 512
#define CONFIG_MAX_MODULES 16
#define CONFIG_MAX_REWRITES 32
#define CONFIG_MAX_TRY_FILES 8

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
  char pattern[CONFIG_MAX_STR];
  char replacement[CONFIG_MAX_STR];
  regex_t re;
} rewrite_rule_t;

#define CONFIG_MAX_SERVERS 16

typedef struct {
  u16 listen_port;
  char server_name[CONFIG_MAX_STR];
  char static_root[CONFIG_MAX_STR];

  struct {
    char paths[CONFIG_MAX_MODULES][CONFIG_MAX_STR];
    int count;
  } modules;

  struct {
    rewrite_rule_t rules[CONFIG_MAX_REWRITES];
    int count;
  } rewrite;

  struct {
    char paths[CONFIG_MAX_TRY_FILES][CONFIG_MAX_STR];
    int count;
  } try_files;

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
    int keepalive_conns;
    backend_entry_t backends[CONFIG_MAX_BACKENDS];
    int backend_count;
  } proxy;

  struct {
    bool enabled;
    char root[CONFIG_MAX_STR];
    int default_ttl;
    int max_entries;
  } cache;

  struct {
    bool enabled;
    int min_length;
  } gzip;
} np_server_config_t;

typedef struct {
  char listen_addr[64];
  int worker_processes;
  int backlog;
  int max_connections;
  int keepalive_timeout;
  int read_timeout;
  int write_timeout;

  np_server_config_t servers[CONFIG_MAX_SERVERS];
  int server_count;

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

  struct {
    bool daemon;
    char pid_file[CONFIG_MAX_STR];
  } process;

  int shutdown_timeout;
} np_config_t;

np_status_t config_load(np_config_t *cfg, const char *path);
void config_destroy(np_config_t *cfg);
void config_print(const np_config_t *cfg);

#endif
