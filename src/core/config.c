#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

static void strip_comment(char *line) {
  char *p = strchr(line, '#');
  if (p) *p = '\0';
  char *end = line + strlen(line);
  while (end > line && isspace((unsigned char)*(end - 1))) {
    end--;
  }
  *end = '\0';
}

static int parse_bool(const char *v) {
  return strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0;
}

np_status_t config_load(np_config_t *cfg, const char *path) {
  memset(cfg, 0, sizeof(*cfg));

  cfg->listen_port = 8080;
  strncpy(cfg->listen_addr, "0.0.0.0", sizeof(cfg->listen_addr) - 1);
  cfg->worker_processes = 4;
  cfg->backlog = 4096;
  cfg->max_connections = 100000;
  cfg->keepalive_timeout = 75;
  cfg->read_timeout = 60;
  cfg->write_timeout = 60;
  strncpy(cfg->static_root, "./www", sizeof(cfg->static_root) - 1);

  cfg->tls.listen_port = 8443;

  cfg->proxy.mode = BALANCE_ROUND_ROBIN;
  cfg->proxy.connect_timeout = 5;
  cfg->proxy.upstream_timeout = 30;

  cfg->rate_limit.requests_per_second = 1000;
  cfg->rate_limit.burst = 200;

  cfg->log.level = LOG_INFO;
  strncpy(cfg->log.access_log, "./logs/access.log", sizeof(cfg->log.access_log) - 1);
  strncpy(cfg->log.error_log, "./logs/error.log", sizeof(cfg->log.error_log) - 1);

  strncpy(cfg->metrics.path, "/metrics", sizeof(cfg->metrics.path) - 1);

  FILE *fp = fopen(path, "r");
  if (!fp) {
    log_error_errno("config_load: cannot open %s", path);
    return NP_ERR_CONFIG;
  }

  char line[512];
  char section[64] = "";

  while (fgets(line, sizeof(line), fp)) {
    strip_comment(line);
    char *p = line;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '\0') continue;

    if (*p == '[') {
      char *end = strchr(p, ']');
      if (!end) continue;
      usize len = (usize)(end - p - 1);
      if (len >= sizeof(section)) len = sizeof(section) - 1;
      memcpy(section, p + 1, len);
      section[len] = '\0';
      continue;
    }

    char *eq = strchr(p, '=');
    if (!eq) continue;
    *eq = '\0';
    char *key = p;
    char *val = eq + 1;

    char *ke = key + strlen(key);
    while (ke > key && isspace((unsigned char)*(ke - 1))) {
      ke--;
      *ke = '\0';
    }
    while (isspace((unsigned char)*val)) val++;
    char *ve = val + strlen(val);
    while (ve > val && isspace((unsigned char)*(ve - 1))) {
      ve--;
      *ve = '\0';
    }

    if (strcmp(section, "server") == 0) {
      if (strcmp(key, "listen_port") == 0)
        cfg->listen_port = (u16)atoi(val);
      else if (strcmp(key, "listen_addr") == 0)
        strncpy(cfg->listen_addr, val, sizeof(cfg->listen_addr) - 1);
      else if (strcmp(key, "worker_processes") == 0)
        cfg->worker_processes = atoi(val);
      else if (strcmp(key, "backlog") == 0)
        cfg->backlog = atoi(val);
      else if (strcmp(key, "max_connections") == 0)
        cfg->max_connections = atoi(val);
      else if (strcmp(key, "keepalive_timeout") == 0)
        cfg->keepalive_timeout = atoi(val);
      else if (strcmp(key, "read_timeout") == 0)
        cfg->read_timeout = atoi(val);
      else if (strcmp(key, "write_timeout") == 0)
        cfg->write_timeout = atoi(val);
      else if (strcmp(key, "static_root") == 0)
        strncpy(cfg->static_root, val, sizeof(cfg->static_root) - 1);
    } else if (strcmp(section, "tls") == 0) {
      if (strcmp(key, "enabled") == 0)
        cfg->tls.enabled = parse_bool(val);
      else if (strcmp(key, "listen_port") == 0)
        cfg->tls.listen_port = (u16)atoi(val);
      else if (strcmp(key, "cert_file") == 0)
        strncpy(cfg->tls.cert_file, val, sizeof(cfg->tls.cert_file) - 1);
      else if (strcmp(key, "key_file") == 0)
        strncpy(cfg->tls.key_file, val, sizeof(cfg->tls.key_file) - 1);
    } else if (strcmp(section, "proxy") == 0) {
      if (strcmp(key, "enabled") == 0)
        cfg->proxy.enabled = parse_bool(val);
      else if (strcmp(key, "mode") == 0)
        cfg->proxy.mode = strcmp(val, "least_conn") == 0 ? BALANCE_LEAST_CONN : BALANCE_ROUND_ROBIN;
      else if (strcmp(key, "connect_timeout") == 0)
        cfg->proxy.connect_timeout = atoi(val);
      else if (strcmp(key, "upstream_timeout") == 0)
        cfg->proxy.upstream_timeout = atoi(val);
    } else if (strcmp(section, "upstream") == 0) {
      if (strcmp(key, "backend") == 0 && cfg->proxy.backend_count < CONFIG_MAX_BACKENDS) {
        backend_entry_t *be = &cfg->proxy.backends[cfg->proxy.backend_count];
        char *colon = strrchr(val, ':');
        if (colon) {
          usize hlen = (usize)(colon - val);
          if (hlen >= sizeof(be->host)) hlen = sizeof(be->host) - 1;
          memcpy(be->host, val, hlen);
          be->host[hlen] = '\0';
          be->port = (u16)atoi(colon + 1);
        } else {
          strncpy(be->host, val, sizeof(be->host) - 1);
          be->port = 80;
        }
        be->enabled = true;
        cfg->proxy.backend_count++;
      }
    } else if (strcmp(section, "rate_limit") == 0) {
      if (strcmp(key, "enabled") == 0)
        cfg->rate_limit.enabled = parse_bool(val);
      else if (strcmp(key, "requests_per_second") == 0)
        cfg->rate_limit.requests_per_second = atoi(val);
      else if (strcmp(key, "burst") == 0)
        cfg->rate_limit.burst = atoi(val);
    } else if (strcmp(section, "log") == 0) {
      if (strcmp(key, "level") == 0) {
        if (strcmp(val, "debug") == 0)
          cfg->log.level = LOG_DEBUG;
        else if (strcmp(val, "info") == 0)
          cfg->log.level = LOG_INFO;
        else if (strcmp(val, "warn") == 0)
          cfg->log.level = LOG_WARN;
        else if (strcmp(val, "error") == 0)
          cfg->log.level = LOG_ERROR;
      } else if (strcmp(key, "access_log") == 0)
        strncpy(cfg->log.access_log, val, sizeof(cfg->log.access_log) - 1);
      else if (strcmp(key, "error_log") == 0)
        strncpy(cfg->log.error_log, val, sizeof(cfg->log.error_log) - 1);
    } else if (strcmp(section, "metrics") == 0) {
      if (strcmp(key, "enabled") == 0)
        cfg->metrics.enabled = parse_bool(val);
      else if (strcmp(key, "path") == 0)
        strncpy(cfg->metrics.path, val, sizeof(cfg->metrics.path) - 1);
    }
  }

  fclose(fp);
  return NP_OK;
}

void config_destroy(np_config_t *cfg) {
  NP_UNUSED(cfg);
}

void config_print(const np_config_t *cfg) {
  log_info("config: listen=%s:%d workers=%d max_conn=%d proxy=%s tls=%s", cfg->listen_addr,
           cfg->listen_port, cfg->worker_processes, cfg->max_connections,
           cfg->proxy.enabled ? "on" : "off", cfg->tls.enabled ? "on" : "off");
}
