#include "config.h"

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

static void strip_comment(char *line) {
  char *p = strchr(line, '#');
  if (p)
    *p = '\0';
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

  strncpy(cfg->listen_addr, "0.0.0.0", sizeof(cfg->listen_addr) - 1);
  cfg->worker_processes = 4;
  cfg->backlog = 4096;
  cfg->max_connections = 100000;
  cfg->keepalive_timeout = 75;
  cfg->read_timeout = 60;
  cfg->write_timeout = 60;

  cfg->server_count = 1;
  np_server_config_t *def = &cfg->servers[0];
  def->listen_port = 8080;
  strncpy(def->static_root, "./www", sizeof(def->static_root) - 1);

  def->tls.listen_port = 8443;

  def->proxy.mode = BALANCE_ROUND_ROBIN;
  def->proxy.connect_timeout = 5;
  def->proxy.upstream_timeout = 30;
  def->proxy.keepalive_conns = 16;

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
  bool seen_server = false;

  while (fgets(line, sizeof(line), fp)) {
    strip_comment(line);
    char *p = line;
    while (isspace((unsigned char)*p))
      p++;
    if (*p == '\0')
      continue;

    if (*p == '[') {
      char *end = strchr(p, ']');
      if (!end)
        continue;
      usize len = (usize)(end - p - 1);
      if (len >= sizeof(section))
        len = sizeof(section) - 1;

      // If we see another [server] block after we already configured the first one, spawn a new
      // server mapping
      if (strncmp(p + 1, "server", 6) == 0) {
        if (seen_server) {
          // Not the first [server] block — allocate a new server slot
          if (cfg->server_count < CONFIG_MAX_SERVERS) {
            cfg->server_count++;
            np_server_config_t *ns = &cfg->servers[cfg->server_count - 1];
            ns->listen_port = 8080;
            strncpy(ns->static_root, "./www", sizeof(ns->static_root) - 1);
          }
        }
        seen_server = true;
      }

      memcpy(section, p + 1, len);
      section[len] = '\0';
      continue;
    }

    char *eq = strchr(p, '=');
    if (!eq)
      continue;
    *eq = '\0';
    char *key = p;
    char *val = eq + 1;

    char *ke = key + strlen(key);
    while (ke > key && isspace((unsigned char)*(ke - 1))) {
      ke--;
      *ke = '\0';
    }
    while (isspace((unsigned char)*val))
      val++;
    char *ve = val + strlen(val);
    while (ve > val && isspace((unsigned char)*(ve - 1))) {
      ve--;
      *ve = '\0';
    }

    np_server_config_t *srv = &cfg->servers[cfg->server_count - 1];

    if (strcmp(section, "server") == 0) {
      if (strcmp(key, "listen_port") == 0)
        srv->listen_port = (u16)atoi(val);
      else if (strcmp(key, "server_name") == 0)
        strncpy(srv->server_name, val, sizeof(srv->server_name) - 1);
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
        strncpy(srv->static_root, val, sizeof(srv->static_root) - 1);
      else if (strcmp(key, "load_module") == 0 && srv->modules.count < CONFIG_MAX_MODULES) {
        strncpy(srv->modules.paths[srv->modules.count], val, CONFIG_MAX_STR - 1);
        srv->modules.count++;
      } else if (strcmp(key, "rewrite") == 0 && srv->rewrite.count < CONFIG_MAX_REWRITES) {
        char *space = strchr(val, ' ');
        if (space) {
          *space = '\0';
          char *repl = space + 1;
          while (isspace((unsigned char)*repl))
            repl++;
          rewrite_rule_t *rule = &srv->rewrite.rules[srv->rewrite.count];
          strncpy(rule->pattern, val, CONFIG_MAX_STR - 1);
          strncpy(rule->replacement, repl, CONFIG_MAX_STR - 1);
          if (regcomp(&rule->re, rule->pattern, REG_EXTENDED) == 0) {
            srv->rewrite.count++;
          } else {
            log_error("config: failed to compile regex '%s'", rule->pattern);
          }
        }
      } else if (strcmp(key, "try_files") == 0) {
        char *token = strtok(val, " ");
        while (token && srv->try_files.count < CONFIG_MAX_TRY_FILES) {
          strncpy(srv->try_files.paths[srv->try_files.count], token, CONFIG_MAX_STR - 1);
          srv->try_files.count++;
          token = strtok(NULL, " ");
        }
      }
    } else if (strcmp(section, "tls") == 0) {
      if (strcmp(key, "enabled") == 0)
        srv->tls.enabled = parse_bool(val);
      else if (strcmp(key, "listen_port") == 0)
        srv->tls.listen_port = (u16)atoi(val);
      else if (strcmp(key, "cert_file") == 0)
        strncpy(srv->tls.cert_file, val, sizeof(srv->tls.cert_file) - 1);
      else if (strcmp(key, "key_file") == 0)
        strncpy(srv->tls.key_file, val, sizeof(srv->tls.key_file) - 1);
    } else if (strcmp(section, "proxy") == 0) {
      if (strcmp(key, "enabled") == 0)
        srv->proxy.enabled = parse_bool(val);
      else if (strcmp(key, "mode") == 0)
        srv->proxy.mode = strcmp(val, "least_conn") == 0 ? BALANCE_LEAST_CONN : BALANCE_ROUND_ROBIN;
      else if (strcmp(key, "connect_timeout") == 0)
        srv->proxy.connect_timeout = atoi(val);
      else if (strcmp(key, "upstream_timeout") == 0)
        srv->proxy.upstream_timeout = atoi(val);
      else if (strcmp(key, "keepalive_conns") == 0)
        srv->proxy.keepalive_conns = atoi(val);
    } else if (strcmp(section, "upstream") == 0) {
      if (strcmp(key, "backend") == 0 && srv->proxy.backend_count < CONFIG_MAX_BACKENDS) {
        backend_entry_t *be = &srv->proxy.backends[srv->proxy.backend_count];
        char *colon = strrchr(val, ':');
        if (colon) {
          usize hlen = (usize)(colon - val);
          if (hlen >= sizeof(be->host))
            hlen = sizeof(be->host) - 1;
          memcpy(be->host, val, hlen);
          be->host[hlen] = '\0';
          be->port = (u16)atoi(colon + 1);
        } else {
          strncpy(be->host, val, sizeof(be->host) - 1);
          be->port = 80;
        }
        be->enabled = true;
        srv->proxy.backend_count++;
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
    } else if (strcmp(section, "cache") == 0) {
      if (strcmp(key, "enabled") == 0)
        srv->cache.enabled = parse_bool(val);
      else if (strcmp(key, "root") == 0)
        strncpy(srv->cache.root, val, sizeof(srv->cache.root) - 1);
      else if (strcmp(key, "default_ttl") == 0)
        srv->cache.default_ttl = atoi(val);
      else if (strcmp(key, "max_entries") == 0)
        srv->cache.max_entries = atoi(val);
    } else if (strcmp(section, "process") == 0) {
      if (strcmp(key, "daemon") == 0)
        cfg->process.daemon = parse_bool(val);
      else if (strcmp(key, "pid_file") == 0)
        strncpy(cfg->process.pid_file, val, sizeof(cfg->process.pid_file) - 1);
    } else if (strcmp(section, "gzip") == 0) {
      if (strcmp(key, "enabled") == 0)
        srv->gzip.enabled = parse_bool(val);
      else if (strcmp(key, "min_length") == 0)
        srv->gzip.min_length = atoi(val);
    }

    if (strcmp(section, "global") == 0 && strcmp(key, "shutdown_timeout") == 0)
      cfg->shutdown_timeout = atoi(val);
  }

  fclose(fp);
  return NP_OK;
}

void config_destroy(np_config_t *cfg) {
  for (int s = 0; s < cfg->server_count; s++) {
    for (int i = 0; s < cfg->servers[s].rewrite.count; i++) {
      regfree(&cfg->servers[s].rewrite.rules[i].re);
    }
  }
}

void config_print(const np_config_t *cfg) {
  log_info("config: listen=%s workers=%d max_conn=%d servers=%d", cfg->listen_addr,
           cfg->worker_processes, cfg->max_connections, cfg->server_count);
}
