/*
 * ProFTPD: mod_redis -- a module for managing Redis data
 * Copyright (c) 2017 The ProFTPD Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 * As a special exemption, TJ Saunders and other respective copyright holders
 * give permission to link this program with OpenSSL, and distribute the
 * resulting executable, without including the source code for OpenSSL in the
 * source distribution.
 *
 * -----DO NOT EDIT BELOW THIS LINE-----
 * $Libraries: -lhiredis$
 */

#include "conf.h"
#include "privs.h"
#include "json.h"
#include "jot.h"

#define MOD_REDIS_VERSION		"mod_redis/0.1"

#if PROFTPD_VERSION_NUMBER < 0x0001030605
# error "ProFTPD 1.3.6rc5 or later required"
#endif

#include <hiredis/hiredis.h>

module redis_module;

#define REDIS_DEFAULT_PORT		6379

static int redis_engine = FALSE;
static int redis_logfd = -1;
static pool *redis_pool = NULL;

static int redis_sess_init(void);

static pr_table_t *jot_logfmt2json = NULL;

static const char *trace_channel = "redis";

static void log_event(cmd_rec *cmd) {
  pr_redis_t *redis;
  config_rec *c;

  redis = pr_redis_conn_get(session.pool);
  if (redis == NULL) {
    (void) pr_log_writefile(redis_logfd, MOD_REDIS_VERSION,
      "error connecting to Redis: %s", strerror(errno));
    return;
  }

  c = find_config(main_server->conf, CONF_PARAM, "RedisLogOnEvent", FALSE);
  while (c != NULL) {
    pool *tmp_pool;
    int res;
    pr_jot_ctx_t *jot_ctx;
    pr_jot_filters_t *jot_filters;
    const char *fmt_name = NULL;
    char *payload = NULL;
    size_t payload_len = 0;
    unsigned char *log_fmt;

    pr_signals_handle();

    tmp_pool = make_sub_pool(cmd->tmp_pool);
    jot_filters = c->argv[0];
    fmt_name = c->argv[1];
    log_fmt = c->argv[2];

    jot_ctx = pcalloc(tmp_pool, sizeof(pr_jot_ctx_t));
    jot_ctx->log = pr_json_object_alloc(tmp_pool);
    jot_ctx->user_data = jot_logfmt2json;

    res = pr_jot_resolve_logfmt(tmp_pool, cmd, jot_filters, log_fmt, jot_ctx,
      pr_jot_on_json, NULL);
    if (res == 0) {
      payload = pr_json_object_to_text(tmp_pool, jot_ctx->log, "");
      payload_len = strlen(payload);
      pr_trace_msg(trace_channel, 8, "generated JSON payload for %s: %s",
        (char *) cmd->argv[0], payload);

    } else {
      (void) pr_log_writefile(redis_logfd, MOD_REDIS_VERSION,
        "error generating JSON formatted log message: %s", strerror(errno));
      payload = NULL;
      payload_len = 0;
    }

    if (payload_len > 0) {
      res = pr_redis_list_append(redis, &redis_module, fmt_name, payload,
        payload_len);
      if (res < 0) {
        (void) pr_log_writefile(redis_logfd, MOD_REDIS_VERSION,
          "error appending log message to '%s': %s", log_fmt, strerror(errno));
      }
    }

    pr_json_object_free(jot_ctx->log);
    destroy_pool(tmp_pool);

    c = find_config_next(c, c->next, CONF_PARAM, "RedisLogOnEvent", FALSE);
  }
}

/* Configuration handlers
 */

/* usage: RedisEngine on|off */
MODRET set_redisengine(cmd_rec *cmd) {
  int engine = -1;
  config_rec *c = NULL;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  engine = get_boolean(cmd, 1);
  if (engine == -1) {
    CONF_ERROR(cmd, "expected Boolean parameter");
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = engine;

  return PR_HANDLED(cmd);
}

/* usage: RedisLog path|"none" */
MODRET set_redislog(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (strcasecmp(cmd->argv[1], "none") != 0 &&
      pr_fs_valid_path(cmd->argv[1]) < 0) {
    CONF_ERROR(cmd, "must be an absolute path");
  }

  add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: RedisLogOnEvent events log-fmt */
MODRET set_redislogonevent(cmd_rec *cmd) {
  config_rec *c, *logfmt_config;
  const char *fmt_name, *rules;
  unsigned char *log_fmt = NULL;
  pr_jot_filters_t *jot_filters;

  CHECK_ARGS(cmd, 2);
  CHECK_CONF(cmd, CONF_ROOT|CONF_GLOBAL|CONF_VIRTUAL);

  c = add_config_param(cmd->argv[0], 3, NULL, NULL, NULL);

  rules = cmd->argv[1];
  jot_filters = pr_jot_filters_create(c->pool, rules,
    PR_JOT_FILTER_OPT_COMMANDS_WITH_CLASSES,
    PR_JOT_FILTER_FL_ALL_INCL_ALL);
  if (jot_filters == NULL) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unable to use events '",
      rules, "': ", strerror(errno), NULL));
  }

  fmt_name = cmd->argv[2];

  /* Make sure that the given LogFormat name is known. */
  logfmt_config = find_config(cmd->server->conf, CONF_PARAM, "LogFormat",
    FALSE);
  while (logfmt_config != NULL) {
    pr_signals_handle();

    if (strcmp(fmt_name, logfmt_config->argv[0]) == 0) {
      log_fmt = logfmt_config->argv[1];
      break;
    }

    logfmt_config = find_config_next(logfmt_config, logfmt_config->next,
      CONF_PARAM, "LogFormat", FALSE);
  }

  if (log_fmt == NULL) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "no LogFormat '", fmt_name,
      "' configured", NULL));
  }

  c->argv[0] = jot_filters;
  c->argv[1] = pstrdup(c->pool, fmt_name);
  c->argv[2] = log_fmt;

  return PR_HANDLED(cmd);
}

/* usage: RedisServer host[:port] ... */
/* NOTE: Need to handle IPv6 addresses here, eventually. */
MODRET set_redisserver(cmd_rec *cmd) {
  config_rec *c;
  char *server, *ptr;
  int ctx, port;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  server = pstrdup(cmd->tmp_pool, cmd->argv[1]);
  ptr = strrchr(server, ':');
  if (ptr == NULL) {
    port = REDIS_DEFAULT_PORT;

  } else {
    *ptr = '\0';
    port = atoi(ptr + 1);
  }

  c = add_config_param(cmd->argv[0], 2, NULL, NULL);
  c->argv[0] = pstrdup(c->pool, server);
  c->argv[1] = palloc(c->pool, sizeof(int));
  *((int *) c->argv[1]) = port;

  ctx = (cmd->config && cmd->config->config_type != CONF_PARAM ?
    cmd->config->config_type : cmd->server->config_type ?
    cmd->server->config_type : CONF_ROOT);

  if (ctx == CONF_ROOT) {
    /* If we're the "server config" context, set the server now.  This
     * would let mod_redis talk to those servers for e.g. ftpdctl actions.
     */
    redis_set_server(server, port);
  }

  return PR_HANDLED(cmd);
}

/* usage: RedisTimeouts conn-timeout io-timeout */
MODRET set_redistimeouts(cmd_rec *cmd) {
  config_rec *c;
  unsigned long connect_millis, io_millis;
  char *ptr = NULL;

  CHECK_ARGS(cmd, 2);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  connect_millis = strtoul(cmd->argv[1], &ptr, 10);
  if (ptr && *ptr) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
      "badly formatted connect timeout value: ", cmd->argv[1], NULL));
  }

  ptr = NULL;
  io_millis = strtoul(cmd->argv[2], &ptr, 10);
  if (ptr && *ptr) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
      "badly formatted IO timeout value: ", cmd->argv[2], NULL));
  }

#if 0
  /* XXX If we're the "server config" context, set the timeouts now.
   * This would let mod_redis talk to those servers for e.g. ftpdctl
   * actions.
   */
  redis_set_timeouts(conn_timeout, io_timeout);
#endif

  c = add_config_param(cmd->argv[0], 2, NULL, NULL);
  c->argv[0] = palloc(c->pool, sizeof(unsigned long));
  *((unsigned long *) c->argv[0]) = connect_millis;
  c->argv[1] = palloc(c->pool, sizeof(unsigned long));
  *((unsigned long *) c->argv[1]) = io_millis;

  return PR_HANDLED(cmd);
}

/* Command handlers
 */

MODRET redis_log_any(cmd_rec *cmd) {
  if (redis_engine == FALSE) {
    return PR_DECLINED(cmd);
  }

  log_event(cmd);
  return PR_DECLINED(cmd);
}

/* Event handlers
 */

static void redis_restart_ev(const void *event_data, void *user_data) {
  destroy_pool(redis_pool);
  redis_pool = make_sub_pool(permanent_pool);
  pr_pool_tag(redis_pool, MOD_REDIS_VERSION);

  jot_logfmt2json = pr_jot_get_logfmt2json_map(redis_pool);
}

static void redis_sess_reinit_ev(const void *event_data, void *user_data) {
  int res;

  /* A HOST command changed the main_server pointer, reinitialize ourselves. */

  pr_event_unregister(&redis_module, "core.session-reinit",
    redis_sess_reinit_ev);

  (void) close(redis_logfd);
  redis_logfd = -1;

  /* XXX Restore other Redis settings? */
  /* reset RedisTimeouts */

  res = redis_sess_init();
  if (res < 0) {
    pr_session_disconnect(&redis_module,
      PR_SESS_DISCONNECT_SESSION_INIT_FAILED, NULL);
  }
}

static void redis_shutdown_ev(const void *event_data, void *user_data) {
  destroy_pool(redis_pool);
  jot_logfmt2json = NULL;
}

/* Initialization functions
 */

static int redis_module_init(void) {
  redis_pool = make_sub_pool(permanent_pool);
  pr_pool_tag(redis_pool, MOD_REDIS_VERSION);

  redis_init();
  pr_event_register(&redis_module, "core.restart", redis_restart_ev, NULL);
  pr_event_register(&redis_module, "core.shutdown", redis_shutdown_ev, NULL);

  pr_log_debug(DEBUG2, MOD_REDIS_VERSION ": using hiredis-%d.%d.%d",
    HIREDIS_MAJOR, HIREDIS_MINOR, HIREDIS_PATCH);

  jot_logfmt2json = pr_jot_get_logfmt2json_map(redis_pool);
  if (jot_logfmt2json == NULL) {
    return -1;
  }

  return 0;
}

static int redis_sess_init(void) {
  config_rec *c;

  pr_event_register(&redis_module, "core.session-reinit",
    redis_sess_reinit_ev, NULL);

  c = find_config(main_server->conf, CONF_PARAM, "RedisEngine", FALSE);
  if (c != NULL) {
    int engine;

    engine = *((int *) c->argv[0]);
    if (engine == FALSE) {
      return 0;
    }

    redis_engine = engine;
  }

  c = find_config(main_server->conf, CONF_PARAM, "RedisLog", FALSE);
  if (c != NULL) {
    const char *path;

    path = c->argv[0];
    if (strcasecmp(path, "none") != 0) {
      int res, xerrno;

      pr_signals_block();
      PRIVS_ROOT
      res = pr_log_openfile(path, &redis_logfd, PR_LOG_SYSTEM_MODE);
      xerrno = errno;
      PRIVS_RELINQUISH
      pr_signals_unblock();

      switch (res) {
        case 0:
          break;

        case -1:
          pr_log_pri(PR_LOG_NOTICE, MOD_REDIS_VERSION
            ": notice: unable to open RedisLog '%s': %s", path,
            strerror(xerrno));
          break;

        case PR_LOG_WRITABLE_DIR:
          pr_log_pri(PR_LOG_WARNING, MOD_REDIS_VERSION
            ": notice: unable to use RedisLog '%s': parent directory is "
              "world-writable", path);
          break;

        case PR_LOG_SYMLINK:
          pr_log_pri(PR_LOG_WARNING, MOD_REDIS_VERSION
            ": notice: unable to use RedisLog '%s': cannot log to a symlink",
            path);
          break;
      }
    }
  }

  c = find_config(main_server->conf, CONF_PARAM, "RedisServer", FALSE);
  if (c != NULL) {
    const char *server;
    int port;

    server = c->argv[0];
    port = *((int *) c->argv[1]);
    redis_set_server(server, port);
  }

  c = find_config(main_server->conf, CONF_PARAM, "RedisTimeouts", FALSE);
  if (c != NULL) {
    unsigned long connect_millis, io_millis;

    connect_millis = *((unsigned long *) c->argv[0]);
    io_millis = *((unsigned long *) c->argv[1]);

    if (redis_set_timeouts(connect_millis, io_millis) < 0) {
      (void) pr_log_writefile(redis_logfd, MOD_REDIS_VERSION,
        "error setting Redis timeouts: %s", strerror(errno));
    }
  }

  return 0;
}

/* Module API tables
 */

static conftable redis_conftab[] = {
  { "RedisEngine",		set_redisengine,	NULL },
  { "RedisLog",			set_redislog,		NULL },
  { "RedisLogOnEvent",		set_redislogonevent,	NULL },
  { "RedisServer",		set_redisserver,	NULL },
  { "RedisTimeouts",		set_redistimeouts,	NULL },
 
  { NULL }
};

static cmdtable redis_cmdtab[] = {
  { LOG_CMD,		C_ANY,	G_NONE,	redis_log_any,	FALSE,	FALSE },
  { LOG_CMD_ERR,	C_ANY,	G_NONE,	redis_log_any,	FALSE,	FALSE },

  { 0, NULL }
};

module redis_module = {
  NULL, NULL,

  /* Module API version 2.0 */
  0x20,

  /* Module name */
  "redis",

  /* Module configuration handler table */
  redis_conftab,

  /* Module command handler table */
  redis_cmdtab,

  /* Module authentication handler table */
  NULL,

  /* Module initialization function */
  redis_module_init,

  /* Session initialization function */
  redis_sess_init,

  /* Module version */
  MOD_REDIS_VERSION
};
