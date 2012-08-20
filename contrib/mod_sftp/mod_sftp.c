/*
 * ProFTPD - mod_sftp
 * Copyright (c) 2008-2012 TJ Saunders
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
 * DO NOT EDIT BELOW THIS LINE
 * $Archive: mod_sftp.a $
 * $Libraries: -lcrypto -lz $
 * $Id: mod_sftp.c,v 1.73 2012-08-20 18:27:33 castaglia Exp $
 */

#include "mod_sftp.h"
#include "ssh2.h"
#include "packet.h"
#include "interop.h"
#include "crypto.h"
#include "keys.h"
#include "keystore.h"
#include "disconnect.h"
#include "kex.h"
#include "blacklist.h"
#include "service.h"
#include "auth.h"
#include "channel.h"
#include "tap.h"
#include "kbdint.h"
#include "fxp.h"
#include "utf8.h"

module sftp_module;

int sftp_logfd = -1;
const char *sftp_logname = NULL;
pool *sftp_pool = NULL;
conn_t *sftp_conn = NULL;
unsigned int sftp_sess_state = 0;
unsigned long sftp_opts = 0UL;
unsigned int sftp_services = SFTP_SERVICE_DEFAULT;

static int sftp_engine = 0;
static const char *sftp_client_version = NULL;
static const char *sftp_server_version = SFTP_ID_DEFAULT_STRING;

static int sftp_have_authenticated(cmd_rec *cmd) {
  return (sftp_sess_state & SFTP_SESS_STATE_HAVE_AUTH);
}

static int sftp_get_client_version(conn_t *conn) {
  int res;

  /* 255 is the RFC-defined maximum banner/ID string size */
  char buf[256];

  /* Read client version.  This looks ugly, reading one byte at a time.
   * It is necesary, though.  The banner sent by the client is not of any
   * guaranteed length.  The client might also send the next SSH packet in
   * the exchange, such that both messages are in the socket buffer.  If
   * we read too much of the banner, we'll read into the KEXINIT, for example,
   * and cause problems later.
   */

  while (TRUE) {
    register unsigned int i;
    int bad_proto = FALSE;

    pr_signals_handle();

    memset(buf, '\0', sizeof(buf));

    for (i = 0; i < sizeof(buf) - 1; i++) {
      res = sftp_ssh2_packet_sock_read(conn->rfd, &buf[i], 1, 0);
      while (res <= 0) {
        if (errno == EINTR) {
          pr_signals_handle();

          res = sftp_ssh2_packet_sock_read(conn->rfd, &buf[i], 1, 0);
          continue;
        }

        if (res < 0) {
          (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
            "error reading from client rfd %d: %s", conn->rfd, strerror(errno));
        }

        return res;
      }

      /* We continue reading until the client has sent the terminating
       * CRLF sequence.
       */
      if (buf[i] == '\r') {
        buf[i] = '\0';
        continue;
      }
 
      if (buf[i] == '\n') {
        buf[i] = '\0';
        break;
      }
    }

    buf[sizeof(buf)-1] = '\0';

    /* If the line does not begin with "SSH-2.0-", skip it.  RFC4253, Section
     * 4.2 does not specify what should happen if the client sends data
     * other than the proper version string initially.
     *
     * If we have been configured for compatibility with old protocol
     * implementations, check for "SSH-1.99-" as well.
     *
     * OpenSSH simply disconnects the client after saying "Protocol mismatch"
     * if the client's version string does not begin with "SSH-2.0-"
     * (or "SSH-1.99-").  Works for me.
     */
    if (strncmp(buf, "SSH-2.0-", 8) != 0) {
      bad_proto = TRUE;

      if (sftp_opts & SFTP_OPT_OLD_PROTO_COMPAT) {
        if (strncmp(buf, "SSH-1.99-", 9) == 0) {
          bad_proto = FALSE;
        }
      } 
    }

    if (bad_proto) {
      const char *errstr = "Protocol mismatch.\n";

      (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
        "Bad protocol version '%.100s' from %s", buf,
        pr_netaddr_get_ipstr(session.c->remote_addr));

      if (write(conn->wfd, errstr, strlen(errstr)) < 0) {
        pr_trace_msg("ssh2", 9,
          "error sending 'Protocol mismatch' message to client: %s",
          strerror(errno));
      }

      errno = EINVAL;
      return -1;
    }

    break;
  }

  sftp_client_version = pstrdup(sftp_pool, buf);
  (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
    "received client version '%s'", sftp_client_version);

  if (sftp_interop_handle_version(sftp_client_version) < 0) {
    (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
      "error checking client version '%s' for interoperability: %s",
      sftp_client_version, strerror(errno));
  }

  return 0;
}

static void sftp_cmd_loop(server_rec *s, conn_t *conn) {
  int res;
  char buf[256];
  const char *k, *v;

  sftp_conn = conn;

  if (sftp_opts & SFTP_OPT_PESSIMISTIC_KEXINIT) {
    /* If we are being pessimistic, we will send our version string to the
     * client now, and send our KEXINIT message later.
     */
    res = sftp_ssh2_packet_send_version();

  } else {
    /* If we are being optimistic, we can reduce the connection latency
     * by sending our KEXINIT message now; this will have the server version
     * string automatically prepended.
     */  
    res = sftp_kex_send_first_kexinit();
  }

  if (res < 0) {
    pr_session_disconnect(&sftp_module, PR_SESS_DISCONNECT_BY_APPLICATION,
      NULL);
  }

  res = sftp_get_client_version(conn);
  if (res < 0) {
    pr_session_disconnect(&sftp_module, PR_SESS_DISCONNECT_BY_APPLICATION,
      NULL);
  }

  sftp_kex_init(sftp_client_version, sftp_server_version);
  sftp_service_init();
  sftp_auth_init();
  sftp_channel_init();

  /* Set the initial timeout for reading packets from clients.  Using
   * a value of zero sets the default timeout value (i.e. TimeoutIdle).
   */
  sftp_ssh2_packet_set_poll_timeout(0);

  k = pstrdup(session.pool, "SFTP");
  v = pstrdup(session.pool, "1");
  pr_env_set(session.pool, k, v);

  k = pstrdup(session.pool, "SFTP_LIBRARY_VERSION");
  v = pstrdup(session.pool, OPENSSL_VERSION_TEXT);
  pr_env_set(session.pool, k, v);

  memset(buf, '\0', sizeof(buf));
  k = pstrdup(session.pool, "SSH_CONNECTION");
  snprintf(buf, sizeof(buf)-1, "%.50s %d %.50s %d",
    pr_netaddr_get_ipstr(conn->remote_addr), conn->remote_port,
    pr_netaddr_get_ipstr(conn->local_addr), conn->local_port);
  v = pstrdup(session.pool, buf);
  pr_env_set(session.pool, k, v);

  /* If we didn't send our KEXINIT earlier, send it now. */
  if (sftp_opts & SFTP_OPT_PESSIMISTIC_KEXINIT) {
    res = sftp_kex_send_first_kexinit();
    if (res < 0) {
      pr_session_disconnect(&sftp_module, PR_SESS_DISCONNECT_BY_APPLICATION,
        NULL);
    }
  }

  while (1) {
    pr_signals_handle();

    res = sftp_ssh2_packet_handle();
    if (res < 0) {
      break;
    }
  }

  return;
}

/* Configuration handlers
 */

/* usage: SFTPAcceptEnv env1 ... envN */
MODRET set_sftpacceptenv(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;
  array_header *accepted_envs;

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  c = add_config_param(cmd->argv[0], 1, NULL);
  accepted_envs = make_array(c->pool, 0, sizeof(char *));

  for (i = 1; i < cmd->argc; i++) {
    *((char **) push_array(accepted_envs)) = pstrdup(c->pool, cmd->argv[i]);
  }
  c->argv[0] = (void *) accepted_envs;

  return PR_HANDLED(cmd);
}

/* usage: SFTPAuthMethods meth1 ... methN */
MODRET set_sftpauthmeths(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;
  char *meths = "";
  unsigned int enabled = 0;

  if (cmd->argc < 2 ||
      cmd->argc > 5) {
    CONF_ERROR(cmd, "Wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  for (i = 1; i < cmd->argc; i++) {
    if (strncasecmp(cmd->argv[i], "publickey", 10) == 0) {
      enabled |= SFTP_AUTH_FL_METH_PUBLICKEY;

    } else if (strncasecmp(cmd->argv[i], "hostbased", 10) == 0) {
      enabled |= SFTP_AUTH_FL_METH_HOSTBASED;

    } else if (strncasecmp(cmd->argv[i], "password", 9) == 0) {
      enabled |= SFTP_AUTH_FL_METH_PASSWORD;

    } else if (strncasecmp(cmd->argv[i], "keyboard-interactive", 21) == 0) {
      if (sftp_kbdint_have_drivers() == 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "unable to support '", cmd->argv[i],
            "' authentication: No drivers loaded", NULL));
      }

      enabled |= SFTP_AUTH_FL_METH_KBDINT;

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
        "unsupported authentication method: ", cmd->argv[i], NULL));
    }
  }

  c = add_config_param(cmd->argv[0], 2, NULL, NULL);

  for (i = 1; i < cmd->argc; i++) {
    meths = pstrcat(c->pool, meths, *meths ? "," : "", cmd->argv[i], NULL);
  }
  c->argv[0] = meths;

  c->argv[1] = pcalloc(c->pool, sizeof(unsigned int));
  *((unsigned int *) c->argv[1]) = enabled;

  return PR_HANDLED(cmd);
}

/* usage: SFTPAuthorized{Host,User}Keys store1 ... */
MODRET set_sftpauthorizedkeys(cmd_rec *cmd) {
  register unsigned int i;
  int requested_key_type = 0;
  config_rec *c;

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (strncasecmp(cmd->argv[0], "SFTPAuthorizedHostKeys", 23) == 0) {
    requested_key_type = SFTP_SSH2_HOST_KEY_STORE;

  } else if (strncasecmp(cmd->argv[0], "SFTPAuthorizedUserKeys", 23) == 0) {
    requested_key_type = SFTP_SSH2_USER_KEY_STORE;
  }

  for (i = 1; i < cmd->argc; i++) {
    char *ptr;

    /* Separate the parameter into its separate store-type:store-info pieces. */
    ptr = strchr(cmd->argv[i], ':');
    if (ptr == NULL) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "badly formatted parameter: '",
        cmd->argv[i], "'", NULL));
    }
    *ptr = '\0';

    /* Verify that the requested store type has been registered, and supports
     * the type of keystore requested (host or user key).
     */
    if (sftp_keystore_supports_store(cmd->argv[i], requested_key_type) < 0) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unsupported key store: '",
        cmd->argv[i], "'", NULL));
    }

    *ptr = ':';
  }

  c = add_config_param(cmd->argv[0], 0);
  c->argc = cmd->argc-1;
  c->argv = pcalloc(c->pool, c->argc * (sizeof(char *)));
  for (i = 1; i < cmd->argc; i++) {
    c->argv[i-1] = pstrdup(c->pool, cmd->argv[i]);
  }

  return PR_HANDLED(cmd);
}

/* usage: SFTPCiphers list */
MODRET set_sftpciphers(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "Wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  for (i = 1; i < cmd->argc; i++) {
    if (sftp_crypto_get_cipher(cmd->argv[i], NULL, NULL) == NULL) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
        "unsupported cipher algorithm: ", cmd->argv[i], NULL));
    }
  }

  c = add_config_param(cmd->argv[0], cmd->argc-1, NULL);
  for (i = 1; i < cmd->argc; i++) {
    c->argv[i-1] = pstrdup(c->pool, cmd->argv[i]);
  }

  return PR_HANDLED(cmd);
}

/* usage: SFTPClientAlive count interval */
MODRET set_sftpclientalive(cmd_rec *cmd) {
  int count, interval;
  config_rec *c;

  CHECK_ARGS(cmd, 2);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  count = atoi(cmd->argv[1]);
  if (count < 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "max count '", cmd->argv[1],
      "' must be equal to or greater than zero", NULL));
  }

  interval = atoi(cmd->argv[2]);
  if (interval < 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "interval '", cmd->argv[2],
      "' must be equal to or greater than zero", NULL));
  }

  c = add_config_param(cmd->argv[0], 2, NULL, NULL);
  c->argv[0] = palloc(c->pool, sizeof(unsigned int));
  *((unsigned int *) c->argv[0]) = count;
  c->argv[1] = palloc(c->pool, sizeof(unsigned int));
  *((unsigned int *) c->argv[1]) = interval;
 
  return PR_HANDLED(cmd);
}

/* usage: SFTPClientMatch pattern key1 val1 ... */
MODRET set_sftpclientmatch(cmd_rec *cmd) {
#ifdef PR_USE_REGEX
  register unsigned int i;
  config_rec *c;
  pr_table_t *tab;
  pr_regex_t *pre;
  int res;

  if (cmd->argc < 4) {
    CONF_ERROR(cmd, "Wrong number of parameters");

  } else {
    int npairs;

    /* Make sure we have an even number of args for the key/value pairs. */

    npairs = cmd->argc - 2;
    if (npairs % 2 != 0) {
      CONF_ERROR(cmd, "Wrong number of parameters");
    }
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  pre = pr_regexp_alloc(&sftp_module);

  res = pr_regexp_compile(pre, cmd->argv[1], REG_EXTENDED|REG_NOSUB);
  if (res != 0) {
    char errstr[200];

    memset(errstr, '\0', sizeof(errstr));
    pr_regexp_error(res, pre, errstr, sizeof(errstr));
    pr_regexp_free(NULL, pre);

    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "'", cmd->argv[1], "' failed regex "
      "compilation: ", errstr, NULL));
  }

  c = add_config_param(cmd->argv[0], 3, NULL, NULL, NULL);
  c->argv[0] = pstrdup(c->pool, cmd->argv[1]);
  c->argv[1] = pre;

  tab = pr_table_alloc(c->pool, 0);

  c->argv[2] = tab;

  for (i = 2; i < cmd->argc; i++) {
    if (strncmp(cmd->argv[i], "channelWindowSize", 18) == 0) {
      uint32_t window_size;
      void *value;
      char *arg, units[3];
      size_t arglen;

      arg = pstrdup(cmd->tmp_pool, cmd->argv[i+1]);
      arglen = strlen(arg);

      memset(units, '\0', sizeof(units));

      if (arglen >= 3) {
        /* Look for any possible "GB", "MB", "KB", "B" suffixes. */

        if ((arg[arglen-2] == 'G' || arg[arglen-2] == 'g') &&
            (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'G';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if ((arg[arglen-2] == 'M' || arg[arglen-2] == 'm') &&
                   (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'M';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if ((arg[arglen-2] == 'K' || arg[arglen-2] == 'k') &&
                   (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'K';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if (arg[arglen-1] == 'B' || arg[arglen-1] == 'b') {
          units[0] = 'B';
          arg[arglen-1] = '\0';
          arglen--;
        }

      } else if (arglen >= 2) {
        /* Look for any possible "B" suffix. */
        if (arg[arglen-1] == 'B' || arg[arglen-1] == 'b') {
          units[0] = 'B';
          arg[arglen-1] = '\0';
          arglen--;
        }
      }

      if (pr_str_get_nbytes(arg, units, (off_t *) &window_size) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error parsing 'channelWindowSize' value ", cmd->argv[i+1], ": ",
          strerror(errno), NULL));
      }

      value = palloc(c->pool, sizeof(uint32_t));
      *((uint32_t *) value) = window_size;

      if (pr_table_add(tab, pstrdup(c->pool, "channelWindowSize"), value,
          sizeof(uint32_t)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'channelWindowSize' value: ", strerror(errno), NULL));
      }

      /* Don't forget to advance i past the value. */
      i++;

    } else if (strncmp(cmd->argv[i], "channelPacketSize", 18) == 0) {
      uint32_t packet_size;
      void *value;
      char *arg, units[3];
      size_t arglen;

      arg = pstrdup(cmd->tmp_pool, cmd->argv[i+1]);
      arglen = strlen(arg);

      memset(units, '\0', sizeof(units));

      if (arglen >= 3) {
        /* Look for any possible "GB", "MB", "KB", "B" suffixes. */

        if ((arg[arglen-2] == 'G' || arg[arglen-2] == 'g') &&
            (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'G';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if ((arg[arglen-2] == 'M' || arg[arglen-2] == 'm') &&
                   (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'M';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if ((arg[arglen-2] == 'K' || arg[arglen-2] == 'k') &&
                   (arg[arglen-1] == 'B' || arg[arglen-1] == 'b')) {
          units[0] = 'K';
          units[1] = 'B';
          arg[arglen-2] = '\0';
          arg[arglen-1] = '\0';
          arglen -= 2;

        } else if (arg[arglen-1] == 'B' || arg[arglen-1] == 'b') {
          units[0] = 'B';
          arg[arglen-1] = '\0';
          arglen--;
        }

      } else if (arglen >= 2) {
        /* Look for any possible "B" suffix. */
        if (arg[arglen-1] == 'B' || arg[arglen-1] == 'b') {
          units[0] = 'B';
          arg[arglen-1] = '\0';
          arglen--;
        }
      }

      if (pr_str_get_nbytes(arg, units, (off_t *) &packet_size) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error parsing 'channelPacketSize' value ", cmd->argv[i+1], ": ",
          strerror(errno), NULL));
      }

      if (packet_size > SFTP_MAX_PACKET_LEN) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "'channelPacketSize' value ", cmd->argv[i+1], " too large, must be "
          "less than 35000B", NULL));
      }

      value = palloc(c->pool, sizeof(uint32_t));
      *((uint32_t *) value) = packet_size;

      if (pr_table_add(tab, pstrdup(c->pool, "channelPacketSize"), value,
          sizeof(uint32_t)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'channelPacketSize' value: ", strerror(errno), NULL));
      }

      /* Don't forget to advance i past the value. */
      i++;

    } else if (strncmp(cmd->argv[i], "pessimisticNewkeys", 19) == 0) {
      int pessimistic_newkeys;
      void *value;

      pessimistic_newkeys = get_boolean(cmd, i+1);
      if (pessimistic_newkeys == -1) {
        CONF_ERROR(cmd, "expected Boolean parameter");
      }

      value = palloc(c->pool, sizeof(int));
      *((int *) value) = pessimistic_newkeys;

      if (pr_table_add(tab, pstrdup(c->pool, "pessimisticNewkeys"), value,
          sizeof(int)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'pessimisticNewkeys' value: ", strerror(errno), NULL));
      }

      /* Don't forget to advance i past the value. */
      i++;

    } else if (strncmp(cmd->argv[i], "sftpProtocolVersion", 20) == 0) {
      void *min_value, *max_value;
      char *ptr = NULL;

      /* Check for a range of values. */
      ptr = strchr(cmd->argv[i+1], '-');

      if (ptr == NULL) {
        long protocol_version;

        /* Just a single value. */

        protocol_version = strtol(cmd->argv[i+1], &ptr, 10);
        if (ptr && *ptr) {
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "badly formatted 'sftpProtocolVersion' value: ", cmd->argv[i+1],
            NULL));
        }

        if (protocol_version < 1 ||
            protocol_version > 6) {
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "'sftpProtocolVersion' value ", cmd->argv[i+1],
            " must be between 1 and 6: ", strerror(errno), NULL));
        }

        min_value = palloc(c->pool, sizeof(unsigned int));
        *((unsigned int *) min_value) = (unsigned int) protocol_version;

        max_value = palloc(c->pool, sizeof(unsigned int));
        *((unsigned int *) max_value) = (unsigned int) protocol_version;

      } else {
        long min_version, max_version;
        char *tmp = NULL;

        /* We have a range of values. */

        *ptr = '\0';
        min_version = strtol(cmd->argv[i+1], &tmp, 10);

        if (tmp && *tmp) {
          *ptr = '-';
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "badly formatted 'sftpProtocolVersion' value: ", cmd->argv[i+1],
            NULL));
        }
        *ptr = '-';

        if (min_version < 1 ||
            min_version > 6) {
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "'sftpProtocolVersion' value ", cmd->argv[i+1],
            " must be between 1 and 6: ", strerror(errno), NULL));
        }

        min_value = palloc(c->pool, sizeof(unsigned int));
        *((unsigned int *) min_value) = (unsigned int) min_version;

        *ptr = '\0';
        tmp = NULL;
        max_version = strtol(ptr + 1, &tmp, 10);

        if (tmp && *tmp) {
          *ptr = '-';
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "badly formatted 'sftpProtocolVersion' value: ", cmd->argv[i+1],
            NULL));
        }
        *ptr = '-';

        if (max_version < 1 ||
            max_version > 6) {
          CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
            "'sftpProtocolVersion' value ", cmd->argv[i+1],
            " must be between 1 and 6: ", strerror(errno), NULL));
        }

        max_value = palloc(c->pool, sizeof(unsigned int));
        *((unsigned int *) max_value) = (unsigned int) max_version;
      }

      if (pr_table_add(tab, pstrdup(c->pool, "sftpMinProtocolVersion"),
          min_value, sizeof(unsigned int)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'sftpProtocolVersion' value: ", strerror(errno),
          NULL));
      }

      if (pr_table_add(tab, pstrdup(c->pool, "sftpMaxProtocolVersion"),
          max_value, sizeof(unsigned int)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'sftpProtocolVersion' value: ", strerror(errno),
          NULL));
      }

      /* Don't forget to advance i past the value. */
      i++;

#ifdef PR_USE_NLS
    } else if (strncmp(cmd->argv[i], "sftpUTF8ProtocolVersion", 24) == 0) {
      char *ptr = NULL;
      void *value;
      long protocol_version;

      protocol_version = strtol(cmd->argv[i+1], &ptr, 10);
      if (ptr && *ptr) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "badly formatted 'sftpUTF8ProtocolVersion' value: ", cmd->argv[i+1],
          NULL));
      }

      if (protocol_version < 1 ||
          protocol_version > 6) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "'sftpUTF8ProtocolVersion' value ", cmd->argv[i+1], NULL));
      }

      value = palloc(c->pool, sizeof(unsigned int));
      *((unsigned int *) value) = (unsigned int) protocol_version;

      if (pr_table_add(tab, pstrdup(c->pool, "sftpUTF8ProtocolVersion"),
          value, sizeof(unsigned int)) < 0) {
        CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
          "error storing 'sftpUTF8ProtocolVersion' value: ", strerror(errno),
          NULL));
      }

      /* Don't forget to advance i past the value. */
      i++;
#endif /* PR_USE_NLS */

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, ": unknown SFTPClientMatch key: '",
        cmd->argv[i], "'", NULL));
    }
  }

  return PR_HANDLED(cmd);

#else /* no regular expression support at the moment */
  CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "The ", cmd->argv[0],
    " directive cannot be used on this system, as you do not have POSIX "
    "compliant regex support", NULL));
#endif
}

/* usage: SFTPCompression on|off|delayed */
MODRET set_sftpcompression(cmd_rec *cmd) {
  config_rec *c;
  int bool;

  if (cmd->argc != 2) {
    CONF_ERROR(cmd, "Wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  bool = get_boolean(cmd, 1);
  if (bool == -1) {
    if (strncasecmp(cmd->argv[1], "delayed", 8) != 0) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
        "unknown compression setting: ", cmd->argv[1], NULL));
    }

    bool = 2;
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = bool;

  return PR_HANDLED(cmd);
}

/* usage: SFTPCryptoDevice engine|"ALL" */
MODRET set_sftpcryptodevice(cmd_rec *cmd) {
#if OPENSSL_VERSION_NUMBER > 0x000907000L
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);

#else /* OpenSSL is too old for ENGINE support */
  CONF_ERROR(cmd, "unsupportable (OpenSSL version is too old");
#endif
}

/* usage: SFTPDHParamFile path */
MODRET set_sftpdhparamfile(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (pr_fs_valid_path(cmd->argv[1]) < 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
      "unable to use '", cmd->argv[1], "'", NULL));
  }

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPDigests list */
MODRET set_sftpdigests(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "Wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  for (i = 1; i < cmd->argc; i++) {
    if (sftp_crypto_get_digest(cmd->argv[i], NULL) == NULL) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
        "unsupported digest algorithm: ", cmd->argv[i], NULL));
    }
  }

  c = add_config_param(cmd->argv[0], cmd->argc-1, NULL);
  for (i = 1; i < cmd->argc; i++) {
    c->argv[i-1] = pstrdup(c->pool, cmd->argv[i]);
  }

  return PR_HANDLED(cmd);
}

/* usage: SFTPDisplayBanner path */
MODRET set_sftpdisplaybanner(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPEngine on|off */
MODRET set_sftpengine(cmd_rec *cmd) {
  int bool = 1;
  config_rec *c;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  bool = get_boolean(cmd, 1);
  if (bool == -1)
    CONF_ERROR(cmd, "expected Boolean parameter");

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = bool;

  return PR_HANDLED(cmd);
}

/* usage: SFTPExtensions ext1 ... extN */
MODRET set_sftpextensions(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;
  unsigned long ext_flags = SFTP_FXP_EXT_DEFAULT;

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  for (i = 1; i < cmd->argc; i++) {
    char action, *ext;

    ext = cmd->argv[i];
    action = *ext;

    if (action != '-' &&
        action != '+') {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "bad option: '", ext, "'",
        NULL));
    }

    ext++;

    if (strncasecmp(ext, "checkFile", 10) == 0) {
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_CHECK_FILE;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_CHECK_FILE;
          break;
      }

    } else if (strncasecmp(ext, "copyFile", 9) == 0) {
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_COPY_FILE;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_COPY_FILE;
          break;
      }

    } else if (strncasecmp(ext, "vendorID", 9) == 0) {
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_VENDOR_ID;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_VENDOR_ID;
          break;
      }

    } else if (strncasecmp(ext, "versionSelect", 14) == 0) {
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_VERSION_SELECT;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_VERSION_SELECT;
          break;
      }

    } else if (strncasecmp(ext, "posixRename", 12) == 0) {
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_POSIX_RENAME;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_POSIX_RENAME;
          break;
      }

    } else if (strncasecmp(ext, "spaceAvailable", 15) == 0) {
#ifdef HAVE_SYS_STATVFS_H
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_SPACE_AVAIL;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_SPACE_AVAIL;
          break;
      }
#else
      pr_log_debug(DEBUG0, "%s: spaceAvailable extension not supported "
        "on this system; requires statvfs(3) support", cmd->argv[0]);
#endif /* !HAVE_SYS_STATVFS_H */


    } else if (strncasecmp(ext, "statvfs", 8) == 0) {
#ifdef HAVE_SYS_STATVFS_H
      switch (action) {
        case '-':
          ext_flags &= ~SFTP_FXP_EXT_STATVFS;
          break;

        case '+':
          ext_flags |= SFTP_FXP_EXT_STATVFS;
          break;
      }
#else
      pr_log_debug(DEBUG0, "%s: statvfs@openssh.com extension not supported "
        "on this system; requires statvfs(3) support", cmd->argv[0]);
#endif /* !HAVE_SYS_STATVFS_H */

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unknown extension: '",
        ext, "'", NULL)); 
    }
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = palloc(c->pool, sizeof(unsigned long));
  *((unsigned long *) c->argv[0]) = ext_flags;

  return PR_HANDLED(cmd);
}

/* usage: SFTPHostKey path|"agent:/..." */
MODRET set_sftphostkey(cmd_rec *cmd) {
  struct stat st;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (strncmp(cmd->argv[1], "agent:", 6) != 0) {
    if (*cmd->argv[1] != '/') {
      CONF_ERROR(cmd, "must be an absolute path");
    }

    if (stat(cmd->argv[1], &st) < 0) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unable to check '", cmd->argv[1],
        "': ", strerror(errno), NULL));
    }

    if ((st.st_mode & S_IRWXG) ||
        (st.st_mode & S_IRWXO)) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unable to use '", cmd->argv[1],
        "' as host key, as it is group- or world-accessible", NULL));
    }
  }

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPKeyBlacklist "none"|path */
MODRET set_sftpkeyblacklist(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (strncasecmp(cmd->argv[1], "none", 5) != 0) {
    if (pr_fs_valid_path(cmd->argv[1]) < 0) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "path '", cmd->argv[1],
        "' not an absolute path", NULL));
    }

    if (!exists(cmd->argv[1])) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "path '", cmd->argv[1],
        "' not found", NULL));
    }
  }

  add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPKeyExchanges list */
MODRET set_sftpkeyexchanges(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;
  char *exchanges = "";

  if (cmd->argc < 2) {
    CONF_ERROR(cmd, "Wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  for (i = 1; i < cmd->argc; i++) {
    if (strncmp(cmd->argv[i], "diffie-hellman-group1-sha1", 27) != 0 &&
        strncmp(cmd->argv[i], "diffie-hellman-group14-sha1", 28) != 0 &&
#if (OPENSSL_VERSION_NUMBER > 0x000907000L && defined(OPENSSL_FIPS)) || \
    (OPENSSL_VERSION_NUMBER > 0x000908000L)
        strncmp(cmd->argv[i], "diffie-hellman-group-exchange-sha256", 37) != 0 &&
#endif
        strncmp(cmd->argv[i], "diffie-hellman-group-exchange-sha1", 35) != 0 &&
        strncmp(cmd->argv[i], "rsa1024-sha1", 13) != 0) {

      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool,
        "unsupported key exchange algorithm: ", cmd->argv[i], NULL));
    }
  }

  c = add_config_param(cmd->argv[0], 1, NULL);

  for (i = 1; i < cmd->argc; i++) {
    exchanges = pstrcat(c->pool, exchanges, *exchanges ? "," : "", cmd->argv[i],
      NULL);
  }
  c->argv[0] = exchanges;

  return PR_HANDLED(cmd);
}

/* usage: SFTPLog path|"none" */
MODRET set_sftplog(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPMaxChannels max */
MODRET set_sftpmaxchannels(cmd_rec *cmd) {
  config_rec *c;
  unsigned int max;
  char *ptr = NULL;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  max = strtoul(cmd->argv[1], &ptr, 10);

  if (ptr && *ptr) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "maximum channel count '",
      cmd->argv[1], "' must be numeric", NULL));
  }

  if (max == 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "maximum channel count '",
      cmd->argv[1], "' must be greater than zero", NULL));
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = palloc(c->pool, sizeof(unsigned int));
  *((unsigned int *) c->argv[0]) = max;

  return PR_HANDLED(cmd);
}

/* usage: SFTPOptions opt1 ... optN */
MODRET set_sftpoptions(cmd_rec *cmd) {
  register unsigned int i;
  config_rec *c;
  unsigned long opts = 0UL;

  if (cmd->argc-1 == 0)
    CONF_ERROR(cmd, "wrong number of parameters");

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  c = add_config_param(cmd->argv[0], 1, NULL);

  for (i = 1; i < cmd->argc; i++) {
    if (strncmp(cmd->argv[i], "IgnoreSFTPUploadPerms", 22) == 0) {
      opts |= SFTP_OPT_IGNORE_SFTP_UPLOAD_PERMS;

    } else if (strncmp(cmd->argv[i], "IgnoreSFTPSetOwners", 19) == 0) {
      opts |= SFTP_OPT_IGNORE_SFTP_SET_OWNERS;

    } else if (strncmp(cmd->argv[i], "IgnoreSFTPSetPerms", 19) == 0) {
      opts |= SFTP_OPT_IGNORE_SFTP_SET_PERMS;

    } else if (strncmp(cmd->argv[i], "IgnoreSFTPSetTimes", 19) == 0) {
      opts |= SFTP_OPT_IGNORE_SFTP_SET_TIMES;

    } else if (strncmp(cmd->argv[i], "IgnoreSCPUploadPerms", 20) == 0) {
      opts |= SFTP_OPT_IGNORE_SCP_UPLOAD_PERMS;

    } else if (strncmp(cmd->argv[i], "OldProtocolCompat", 18) == 0) {
      opts |= SFTP_OPT_OLD_PROTO_COMPAT;

      /* This option also automatically enables PessimisticKexint,
       * as per the comments in RFC4253, Section 5.1.
       */
      opts |= SFTP_OPT_PESSIMISTIC_KEXINIT;
 
    } else if (strncmp(cmd->argv[i], "PessimisticKexinit", 19) == 0) {
      opts |= SFTP_OPT_PESSIMISTIC_KEXINIT;

    } else if (strncmp(cmd->argv[i], "MatchKeySubject", 16) == 0) {
      opts |= SFTP_OPT_MATCH_KEY_SUBJECT;

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, ": unknown SFTPOption '",
        cmd->argv[i], "'", NULL));
    }
  }

  c->argv[0] = pcalloc(c->pool, sizeof(unsigned long));
  *((unsigned long *) c->argv[0]) = opts;

  return PR_HANDLED(cmd);
}

/* usage: SFTPPassPhraseProvider path */
MODRET set_sftppassphraseprovider(cmd_rec *cmd) {
  struct stat st;
 
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT);
 
  if (*cmd->argv[1] != '/') {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "must be a full path: '",
      cmd->argv[1], "'", NULL));
  }
 
  if (stat(cmd->argv[1], &st) < 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "error checking '", 
      cmd->argv[1], "': ", strerror(errno), NULL));
  }

  if (!S_ISREG(st.st_mode)) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unable to use '",
      cmd->argv[1], ": Not a regular file", NULL));
  }

  add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: SFTPRekey "none"|"required" [interval bytes [timeout]] */
MODRET set_sftprekey(cmd_rec *cmd) {
  config_rec *c;
  int rekey_interval;
  unsigned long rekey_mbytes;
  char *ptr = NULL;

  if (cmd->argc-1 < 1 || cmd->argc-1 > 4) {
    CONF_ERROR(cmd, "wrong number of parameters");
  }

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (strncasecmp(cmd->argv[1], "none", 5) == 0) {
    c = add_config_param(cmd->argv[0], 1, NULL);
    c->argv[0] = pcalloc(c->pool, sizeof(int));
    *((int *) c->argv[0]) = FALSE;

    return PR_HANDLED(cmd);
  }

  if (strncasecmp(cmd->argv[1], "required", 9) != 0) {
    CONF_ERROR(cmd, "expected either 'none' or 'required'");
  }

  if (cmd->argc-1 == 4) {
    /* The admin specified a rekey timeout as well.  Nice. */
    c = add_config_param(cmd->argv[0], 4, NULL, NULL, NULL, NULL);

  } else {
    /* The admin did not specify rekey timeout as well.  Oh well. */
    c = add_config_param(cmd->argv[0], 3, NULL, NULL, NULL);
  }

  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = TRUE;

  if (cmd->argc-1 >= 2) {
    rekey_interval = atoi(cmd->argv[2]);

  } else {
    /* Default: one hour. */
    rekey_interval = 3600;
  }

  if (rekey_interval > 0) {
    c->argv[1] = pcalloc(c->pool, sizeof(int));
    *((int *) c->argv[1]) = rekey_interval;

  } else {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "rekey interval '", cmd->argv[2],
      "' must be greater than zero", NULL));
  }

  if (cmd->argc-1 >= 3) {
    rekey_mbytes = strtoul(cmd->argv[3], &ptr, 10);
    if (ptr && *ptr) {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "rekey MB '", cmd->argv[3],
        "' must be numeric", NULL));
    }

  } else {
    /* Default: 2 GB */
    rekey_mbytes = (2 * 1024);
  }

  c->argv[2] = pcalloc(c->pool, sizeof(off_t));
  *((off_t *) c->argv[2]) = (off_t) rekey_mbytes * 1024 * 1024;

  if (cmd->argc-1 == 4) {
    int rekey_timeout;

    rekey_timeout = atoi(cmd->argv[4]);
    if (rekey_timeout > 0) {
      c->argv[3] = pcalloc(c->pool, sizeof(int));
      *((int *) c->argv[3]) = rekey_timeout;

    } else {
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "rekey timeout '", cmd->argv[4],
        "' must be greater than zero", NULL));
    }
  }

  return PR_HANDLED(cmd);
}

/* usage: SFTPTrafficPolicy policy */
MODRET set_sftptrafficpolicy(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if (sftp_tap_have_policy(cmd->argv[1]) < 0) {
    CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "'", cmd->argv[1],
      "' is not a recognized policy", NULL));
  }

  (void) add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* Event handlers
 */

extern pid_t mpid;

static void sftp_exit_ev(const void *event_data, void *user_data) {

  /* Close any channels/sessions that remain open. */
  sftp_channel_free();

  sftp_keys_free();
  sftp_kex_free();

  sftp_crypto_free(0);
  sftp_utf8_free();

  if (sftp_logfd >= 0) {
    (void) close(sftp_logfd);
    sftp_logfd = -1;
  }
}

static void sftp_ban_class_ev(const void *event_data, void *user_data) {
  const char *proto;

  proto = pr_session_get_protocol(PR_SESS_PROTO_FL_LOGOUT);

  /* Only send an SSH2 DISCONNECT if we're dealing with an SSH2 client. */
  if (strncmp(proto, "SSH2", 5) == 0) {
    sftp_disconnect_send(SFTP_SSH2_DISCONNECT_BY_APPLICATION, "Banned",
      __FILE__, __LINE__, "");
  }
}

static void sftp_ban_host_ev(const void *event_data, void *user_data) {
  const char *proto;

  proto = pr_session_get_protocol(PR_SESS_PROTO_FL_LOGOUT);

  /* Only send an SSH2 DISCONNECT if we're dealing with an SSH2 client. */
  if (strncmp(proto, "SSH2", 5) == 0) {
    sftp_disconnect_send(SFTP_SSH2_DISCONNECT_BY_APPLICATION, "Banned",
      __FILE__, __LINE__, "");
  }
}

static void sftp_ban_user_ev(const void *event_data, void *user_data) {
  const char *proto;

  proto = pr_session_get_protocol(PR_SESS_PROTO_FL_LOGOUT);

  /* Only send an SSH2 DISCONNECT if we're dealing with an SSH2 client. */
  if (strncmp(proto, "SSH2", 5) == 0) {
    sftp_disconnect_send(SFTP_SSH2_DISCONNECT_BY_APPLICATION, "Banned",
      __FILE__, __LINE__, "");
  }
}

static void sftp_max_conns_ev(const void *event_data, void *user_data) {
  const char *proto;

  proto = pr_session_get_protocol(PR_SESS_PROTO_FL_LOGOUT);

  /* Only send an SSH2 DISCONNECT if we're dealing with an SSH2 client. */
  if (strncmp(proto, "SSH2", 5) == 0) {
    sftp_disconnect_send(SFTP_SSH2_DISCONNECT_TOO_MANY_CONNECTIONS,
      "Maximum connections for host/user reached", __FILE__, __LINE__, "");
  }
}

#if defined(PR_SHARED_MODULE)
static void sftp_mod_unload_ev(const void *event_data, void *user_data) {
  if (strncmp((const char *) event_data, "mod_sftp.c", 11) == 0) {
    /* Unregister ourselves from all events. */
    pr_event_unregister(&sftp_module, NULL, NULL);

    sftp_interop_free();
    sftp_keystore_free();
    sftp_keys_free();
    pr_response_block(FALSE);
    sftp_utf8_free();

    /* Clean up the OpenSSL stuff. */
    sftp_crypto_free(0);

    destroy_pool(sftp_pool);
    sftp_pool = NULL;

    close(sftp_logfd);
    sftp_logfd = -1;
  }
}
#endif

static void sftp_postparse_ev(const void *event_data, void *user_data) {
  config_rec *c;

  /* Initialize OpenSSL. */
  ERR_load_crypto_strings();
  OpenSSL_add_all_algorithms();

  c = find_config(main_server->conf, CONF_PARAM, "SFTPPassPhraseProvider",
    FALSE);
  if (c) {
    sftp_keys_set_passphrase_provider(c->argv[0]);
  }

  sftp_keys_get_passphrases();
}

static void sftp_restart_ev(const void *event_data, void *user_data) {

  /* Clear the host keys. */
  sftp_keys_free();

  /* Re-initialize the interoperability checks.  A restart clears the memory
   * pool used by the compiled regexes, hence the need to re-compile them.
   */
  if (sftp_interop_init() < 0) {
    pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION
      ": error preparing interoperability checks: %s", strerror(errno));
  }
}

static void sftp_shutdown_ev(const void *event_data, void *user_data) {
  sftp_interop_free();
  sftp_keystore_free();
  sftp_keys_free();
  sftp_utf8_free();

  /* Clean up the OpenSSL stuff. */
  sftp_crypto_free(0);

  destroy_pool(sftp_pool);
  sftp_pool = NULL;

  if (sftp_logfd >= 0) {
    close(sftp_logfd);
    sftp_logfd = -1;
  }
}

static void sftp_wrap_conn_denied_ev(const void *event_data, void *user_data) {
  const char *proto;

  proto = pr_session_get_protocol(PR_SESS_PROTO_FL_LOGOUT);

  /* Only send an SSH2 DISCONNECT if we're dealing with an SSH2 client. */
  if (strncmp(proto, "SSH2", 5) == 0) {
    char *msg;

    msg = get_param_ptr(main_server->conf, "WrapDenyMsg", FALSE);
    if (msg != NULL) {
      /* If the client has authenticated, we can interpolate any '%u'
       * variable in the configured deny message.
       */
      if (sftp_sess_state & SFTP_SESS_STATE_HAVE_AUTH) {
        msg = sreplace(sftp_pool, msg, "%u", session.user, NULL);
      }

    } else {
      /* XXX This needs to be properly localized.  However, trying to use
       * the _("") construction here causes mod_sftp.c to fail compilation
       * (see Bug#3677), so leave it hardcoded for now.
       */
      msg = "Access denied";
    }

    /* If the client has completed the KEXINIT, we can simply use
     * sftp_disconnect_send().
     */
    if (sftp_sess_state & SFTP_SESS_STATE_HAVE_KEX) {
      sftp_disconnect_send(SFTP_SSH2_DISCONNECT_BY_APPLICATION, msg,
        __FILE__, __LINE__, "");

    } else {
      /* If the client has not completed the KEXINIT, then just send the
       * disconnected message, if any, directly.  Make sure to terminate
       * the message with a newline character.
       */
      msg = pstrcat(sftp_pool, msg, "\n", NULL);

      /* Make sure we block the Response API, otherwise mod_wrap/mod_wrap2 will
       * also be sending its response, and the SSH client may be confused.
       */
      pr_response_block(TRUE);

      if (write(session.c->wfd, msg, strlen(msg)) < 0) {
        pr_trace_msg("ssh2", 9,
          "error sending mod_wrap2 connection denied message to client: %s",
          strerror(errno));
      }
    }
  }
}

/* Initialization routines
 */

static int sftp_init(void) {
  unsigned long openssl_version;

  /* Check that the OpenSSL headers used match the version of the
   * OpenSSL library used.
   *
   * For now, we only log if there is a difference.
   */
  openssl_version = SSLeay();

  if (openssl_version != OPENSSL_VERSION_NUMBER) {
    int unexpected_version_mismatch = TRUE;

    if (OPENSSL_VERSION_NUMBER >= 0x1000000fL) {
      /* OpenSSL versions after 1.0.0 try to maintain ABI compatibility.
       * So we will warn about header/library version mismatches only if
       * the library is older than the headers.
       */
      if (openssl_version >= OPENSSL_VERSION_NUMBER) {
        unexpected_version_mismatch = FALSE;
      }
    }

    if (unexpected_version_mismatch == TRUE) {
      pr_log_pri(PR_LOG_ERR, MOD_SFTP_VERSION
        ": compiled using OpenSSL version '%s' headers, but linked to "
        "OpenSSL version '%s' library", OPENSSL_VERSION_TEXT,
        SSLeay_version(SSLEAY_VERSION));
    }
  }

  pr_log_debug(DEBUG2, MOD_SFTP_VERSION ": using " OPENSSL_VERSION_TEXT);

  /* Initialize the interoperability checks here, so that all session
   * processes share the compiled regexes in memory.
   */
  if (sftp_interop_init() < 0) {
    pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION
      ": error preparing interoperability checks: %s", strerror(errno));
    return -1;
  }

  sftp_keystore_init();

  pr_event_register(&sftp_module, "mod_ban.ban-class", sftp_ban_class_ev, NULL);
  pr_event_register(&sftp_module, "mod_ban.ban-host", sftp_ban_host_ev, NULL);
  pr_event_register(&sftp_module, "mod_ban.ban-user", sftp_ban_user_ev, NULL);

  /* Listen for mod_wrap/mod_wrap2 connection denied events, so that we can
   * attempt to display any deny messages from those modules to the connecting
   * SSH2 client.
   */
  pr_event_register(&sftp_module, "mod_wrap.connection-denied",
    sftp_wrap_conn_denied_ev, NULL);

#if defined(PR_SHARED_MODULE)
  pr_event_register(&sftp_module, "core.module-unload", sftp_mod_unload_ev,
    NULL);
#endif
  pr_event_register(&sftp_module, "core.postparse", sftp_postparse_ev, NULL);
  pr_event_register(&sftp_module, "core.restart", sftp_restart_ev, NULL);
  pr_event_register(&sftp_module, "core.shutdown", sftp_shutdown_ev, NULL);

  return 0;
}

static int sftp_sess_init(void) {
  config_rec *c;
  int times_gmt = TRUE;

  c = find_config(main_server->conf, CONF_PARAM, "SFTPEngine", FALSE);
  if (c) {
    sftp_engine = *((int *) c->argv[0]);
  }

  if (!sftp_engine)
    return 0;

  pr_event_register(&sftp_module, "core.exit", sftp_exit_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-clients",
    sftp_max_conns_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-clients-per-class",
    sftp_max_conns_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-clients-per-host",
    sftp_max_conns_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-clients-per-user",
    sftp_max_conns_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-connections-per-host",
    sftp_max_conns_ev, NULL);
  pr_event_register(&sftp_module, "mod_auth.max-hosts-per-user",
    sftp_max_conns_ev, NULL);

  c = find_config(main_server->conf, CONF_PARAM, "SFTPLog", FALSE);
  if (c) {
    int res;

    sftp_logname = c->argv[0];

    pr_signals_block();
    PRIVS_ROOT
    res = pr_log_openfile(sftp_logname, &sftp_logfd, PR_LOG_SYSTEM_MODE);
    PRIVS_RELINQUISH
    pr_signals_unblock();

    if (res < 0) {
      if (res == -1) {
        pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION
          ": notice: unable to open SFTPLog '%s': %s", sftp_logname,
          strerror(errno));

      } else if (res == PR_LOG_WRITABLE_DIR) {
        pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION
          ": notice: unable to open SFTPLog '%s': parent directory is "
          "world-writable", sftp_logname);

      } else if (res == PR_LOG_SYMLINK) {
        pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION
          ": notice: unable to open SFTPLog '%s': cannot log to a symlink",
          sftp_logname);
      }
    }
  }

  if (pr_define_exists("SFTP_USE_FIPS")) {
#ifdef OPENSSL_FIPS
    if (!FIPS_mode()) {

      /* Make sure OpenSSL is set to use the default RNG, as per an email
       * discussion on the OpenSSL developer list:
       *
       *  "The internal FIPS logic uses the default RNG to see the FIPS RNG
       *   as part of the self test process..." 
       */ 
      RAND_set_rand_method(NULL);

      if (!FIPS_mode_set(1)) { 
        const char *errstr;

        errstr = sftp_crypto_get_errors();
        (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
          "unable to use FIPS mode: %s", errstr);
        pr_log_pri(PR_LOG_ERR, MOD_SFTP_VERSION ": unable to use FIPS mode: %s",
          errstr);

        errno = EACCES;
        return -1;

      } else {
        pr_log_pri(PR_LOG_NOTICE, MOD_SFTP_VERSION ": FIPS mode enabled");
      }

    } else {
      pr_log_pri(PR_LOG_DEBUG, MOD_SFTP_VERSION ": FIPS mode already enabled");
    }
#else
    pr_log_pri(PR_LOG_WARNING, MOD_SFTP_VERSION ": FIPS mode requested, but " OPENSSL_VERSION_TEXT " not built with FIPS support");
#endif /* OPENSSL_FIPS */
  }

#if OPENSSL_VERSION_NUMBER > 0x000907000L
  /* Handle any requested crypto accelerators/drivers. */
  c = find_config(main_server->conf, CONF_PARAM, "SFTPCryptoDevice", FALSE);
  if (c) {
    if (sftp_crypto_set_driver(c->argv[0]) < 0) {
      (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
        "unable use SFTPCryptoDevice '%s': %s", (const char *) c->argv[0],
        strerror(errno));
    }
  }
#endif

  sftp_pool = make_sub_pool(session.pool);
  pr_pool_tag(sftp_pool, MOD_SFTP_VERSION);

  c = find_config(main_server->conf, CONF_PARAM, "SFTPHostKey", FALSE);
  while (c) {
    const char *path = c->argv[0];

    /* This pool needs to have the lifetime of the session, since the hostkey
     * data is needed for rekeying, and rekeying can happen at any time
     * during the session.
     */
    if (sftp_keys_get_hostkey(sftp_pool, path) < 0) {
      (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
        "error loading hostkey '%s', skipping key", path);
    }

    c = find_config_next(c, c->next, CONF_PARAM, "SFTPHostKey", FALSE);
  }

  /* Support having either an RSA hostkey, a DSA hostkey, or both.  But
   * we have to have at least one hostkey.
   */
  if (sftp_keys_have_dsa_hostkey() < 0 &&
      sftp_keys_have_rsa_hostkey() < 0) {
    (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
      "no available host keys, unable to handle session");
    errno = EACCES;
    return -1;
  }

  c = find_config(main_server->conf, CONF_PARAM, "SFTPKeyBlacklist", FALSE);
  if (c) {
    if (strncasecmp((char *) c->argv[0], "none", 5) != 0) {
      sftp_blacklist_set_file(c->argv[0]);

    } else {
      /* Admin explicitly requested no checking of a key blacklist. */
      sftp_blacklist_set_file(NULL);
    }
  }

  c = find_config(main_server->conf, CONF_PARAM, "SFTPMaxChannels", FALSE);
  if (c) {
    sftp_channel_set_max_count(*((unsigned int *) c->argv[0]));
  }

  c = find_config(main_server->conf, CONF_PARAM, "SFTPOptions", FALSE);
  while (c != NULL) {
    unsigned long opts;

    pr_signals_handle();

    opts = *((unsigned long *) c->argv[0]);
    sftp_opts |= opts;

    c = find_config_next(c, c->next, CONF_PARAM, "SFTPOptions", FALSE);
  }

  c = find_config(main_server->conf, CONF_PARAM, "DisplayLogin", FALSE);
  if (c) {
    const char *path;

    path = c->argv[0];
    if (sftp_fxp_set_displaylogin(path) < 0) {
      (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
        "error using DisplayLogin '%s': %s", path, strerror(errno));
    }
  }

  c = find_config(main_server->conf, CONF_PARAM, "ServerIdent", FALSE);
  if (c) {
    if (*((unsigned char *) c->argv[0]) == FALSE) {
      /* The admin configured "ServerIdent off".  Set the version string to
       * just "mod_sftp", and that's it, no version.
       */
      sftp_server_version = pstrcat(sftp_pool, SFTP_ID_PREFIX, "mod_sftp",
        NULL);
      sftp_ssh2_packet_set_version(sftp_server_version);

    } else {
      /* The admin configured "ServerIdent on", and possibly some custom
       * string.
       */
      if (c->argc > 1) {
        sftp_server_version = pstrcat(sftp_pool, SFTP_ID_PREFIX, c->argv[1],
          NULL);
        sftp_ssh2_packet_set_version(sftp_server_version);
      }
    }
  }

  c = find_config(main_server->conf, CONF_PARAM, "TimesGMT", FALSE);
  if (c) {
    times_gmt = *((unsigned char *) c->argv[0]);
  }

  pr_response_block(TRUE);

  c = find_config(main_server->conf, CONF_PARAM, "SFTPExtensions", FALSE);
  if (c) {
    sftp_fxp_set_extensions(*((unsigned long *) c->argv[0]));
  }

  sftp_fxp_use_gmt(times_gmt);

  c = find_config(main_server->conf, CONF_PARAM, "SFTPClientAlive", FALSE);
  if (c) {
    unsigned int count, interval;

    count = *((unsigned int *) c->argv[0]);
    interval = *((unsigned int *) c->argv[1]);

    (void) sftp_ssh2_packet_set_client_alive(count, interval);

    pr_trace_msg("ssh2", 7,
      "client alive checks requested after %u secs, up to %u times",
      interval, count);
  }

  /* Check for any rekey policy. */
  c = find_config(main_server->conf, CONF_PARAM, "SFTPRekey", FALSE);
  if (c) {
    int rekey;

    /* The possible int values here are:
     *
     * 0 (disable rekeying)
     * 1 (enable rekeying, with parameters)
     */

    rekey = *((int *) c->argv[0]);
    if (rekey) {
      int rekey_interval;
      off_t rekey_size;

      rekey_interval = *((int *) c->argv[1]);
      rekey_size = *((off_t *) c->argv[2]);

      pr_trace_msg("ssh2", 6, "SSH2 rekeys requested after %d secs "
        "or %" PR_LU " bytes", rekey_interval, (pr_off_t) rekey_size);
      sftp_kex_rekey_set_interval(rekey_interval);
      sftp_ssh2_packet_rekey_set_size(rekey_size);

      if (c->argc == 4) {
        int rekey_timeout;

        rekey_timeout = *((int *) c->argv[3]);

        pr_trace_msg("ssh2", 6, "SSH2 rekeying has %d %s to complete",
          rekey_timeout, rekey_timeout != 1 ? "secs" : "sec");
        sftp_kex_rekey_set_timeout(rekey_timeout);
      }

    } else {
      sftp_kex_rekey_set_interval(0);
      sftp_kex_rekey_set_timeout(0);
      sftp_ssh2_packet_rekey_set_seqno(0);
      sftp_ssh2_packet_rekey_set_size(0);

      pr_trace_msg("ssh2", 6,
        "SSH2 server-requested rekeys disabled by SFTPRekey");
    }

  } else {

    /* Set the default rekey values: 1 hour (3600 secs) and 2 GB.
     * Also, as per RFC4344, rekeys will be requested whenever the
     * packet sequence numbers reach rollover; these are handled by
     * default in packet.c.
     */
    sftp_kex_rekey_set_interval(3600);
    sftp_ssh2_packet_rekey_set_size((off_t) 2147483648UL);
  }

  /* Enable traffic analysis protection (TAP) after keys have been
   * exchanged, based on the configured policy.
   */
  c = find_config(main_server->conf, CONF_PARAM, "SFTPTrafficPolicy", FALSE);
  if (c) {
    const char *policy = c->argv[0];

    if (sftp_tap_set_policy(policy) < 0) {
      (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
        "error setting TrafficPolicy '%s': %s", policy, strerror(errno));

    } else {
      pr_trace_msg("ssh2", 9, "using TAP policy '%s'", policy);
    }
  }

  /* Use our own "authenticated yet?" check. */
  set_auth_check(sftp_have_authenticated);

  pr_session_set_protocol("ssh2");
  pr_cmd_set_handler(sftp_cmd_loop);

  /* Check for any UseEncoding directives.  Specifically, we're interested
   * in the charset portion; the encoding is always UTF8 for SFTP clients
   * (when applicable).
   */

  c = find_config(main_server->conf, CONF_PARAM, "UseEncoding", FALSE);
  if (c) {
    if (c->argc == 2) {
      char *charset;

      charset = c->argv[0];

      if (sftp_utf8_set_charset(charset) < 0) {
        (void) pr_log_writefile(sftp_logfd, MOD_SFTP_VERSION,
          "error setting local charset '%s': %s", charset, strerror(errno));

        /* Re-initialize the UTF8 conversion handles. */
        (void) sftp_utf8_free();
        sftp_utf8_init();
      }

    } else {
      sftp_utf8_init();
    }

  } else {
    sftp_utf8_init();
  }

  return 0;
}

/* Module API tables
 */

static conftable sftp_conftab[] = {
  { "SFTPAcceptEnv",		set_sftpacceptenv,		NULL },
  { "SFTPAuthMethods",		set_sftpauthmeths,		NULL },
  { "SFTPAuthorizedHostKeys",	set_sftpauthorizedkeys,		NULL },
  { "SFTPAuthorizedUserKeys",	set_sftpauthorizedkeys,		NULL },
  { "SFTPCiphers",		set_sftpciphers,		NULL },
  { "SFTPClientAlive",		set_sftpclientalive,		NULL },
  { "SFTPClientMatch",		set_sftpclientmatch,		NULL },
  { "SFTPCompression",		set_sftpcompression,		NULL },
  { "SFTPCryptoDevice",		set_sftpcryptodevice,		NULL },
  { "SFTPDHParamFile",		set_sftpdhparamfile,		NULL },
  { "SFTPDigests",		set_sftpdigests,		NULL },
  { "SFTPDisplayBanner",	set_sftpdisplaybanner,		NULL },
  { "SFTPEngine",		set_sftpengine,			NULL },
  { "SFTPExtensions",		set_sftpextensions,		NULL },
  { "SFTPHostKey",		set_sftphostkey,		NULL },
  { "SFTPKeyBlacklist",		set_sftpkeyblacklist,		NULL },
  { "SFTPKeyExchanges",		set_sftpkeyexchanges,		NULL },
  { "SFTPLog",			set_sftplog,			NULL },
  { "SFTPMaxChannels",		set_sftpmaxchannels,		NULL },
  { "SFTPOptions",		set_sftpoptions,		NULL },
  { "SFTPPassPhraseProvider",	set_sftppassphraseprovider,	NULL },
  { "SFTPRekey",		set_sftprekey,			NULL },
  { "SFTPTrafficPolicy",	set_sftptrafficpolicy,		NULL },
  { NULL }
};

module sftp_module = {
  /* Always NULL */
  NULL, NULL,

  /* Module API version */
  0x20,

  /* Module name */
  "sftp",

  /* Module configuration handler table */
  sftp_conftab,

  /* Module command handler table */
  NULL,

  /* Module authentication handler table */
  NULL,

  /* Module initialization */
  sftp_init,

  /* Session initialization */
  sftp_sess_init,

  /* Module version */
  MOD_SFTP_VERSION
};

