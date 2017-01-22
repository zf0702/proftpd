/*
 * ProFTPD - FTP server daemon
 * Copyright (c) 2017 The ProFTPD Project team
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
 */

#include "conf.h"
#include "logfmt.h"
#include "json.h"
#include "jot.h"

struct jot_filters_rec {
  pool *pool;

  int included_classes;
  int excluded_classes;
  array_header *cmd_ids;
};

/* For tracking the size of deleted files. */
static off_t jot_deleted_filesz = 0;

static const char *trace_channel = "jot";

/* Entries in the JSON map table identify the key, and the data type:
 * Boolean, number, or string.
 */
struct logfmt_json_info {
  unsigned int json_type;
  const char *json_key;
  size_t json_keylen;
};

/* Key comparison for the ID/key table. */
static int logfmt_json_keycmp(const void *k1, size_t ksz1, const void *k2,
  size_t ksz2) {

  /* Return zero to indicate a match, non-zero otherwise. */
  return (*((unsigned char *) k1) == *((unsigned char *) k2) ? 0 : 1);
}

/* Key "hash" callback for ID/key table. */
static unsigned int logfmt_json_keyhash(const void *k, size_t ksz) {
  unsigned char c;
  unsigned int res;

  c = *((unsigned char *) k);
  res = (c << 8);

  return res;
}

static int add_json_info(pool *p, pr_table_t *tab,
    unsigned char logfmt_id, const char *json_key, unsigned int json_type) {
  unsigned char *k;
  struct logfmt_json_info *lji;
  int res;

  k = palloc(p, sizeof(unsigned char));
  *k = logfmt_id;

  lji = palloc(p, sizeof(struct logfmt_json_info));
  lji->json_type = json_type;
  lji->json_key = json_key;
  lji->json_keylen = strlen(json_key) + 1;

  res = pr_table_kadd(tab, (const void *) k, sizeof(unsigned char),
    lji, sizeof(struct logfmt_json_info *));
  return res;
}

pr_table_t *pr_jot_get_logfmt2json(pool *p) {
  pr_table_t *map;

  if (p == NULL) {
    errno = EINVAL;
    return NULL;
  }

  map = pr_table_alloc(p, 0);

  (void) pr_table_ctl(map, PR_TABLE_CTL_SET_KEY_CMP,
    (void *) logfmt_json_keycmp);
  (void) pr_table_ctl(map, PR_TABLE_CTL_SET_KEY_HASH,
    (void *) logfmt_json_keyhash);

  /* Now populate the map with the ID/name values.  The key is the
   * LogFormat "meta" ID, and the value is the corresponding name string,
   * for use e.g. as JSON object member names.
   */

  add_json_info(p, map, LOGFMT_META_BYTES_SENT, PR_JOT_LOGFMT_BYTES_SENT_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_FILENAME, PR_JOT_LOGFMT_FILENAME_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_ENV_VAR, PR_JOT_LOGFMT_ENV_VAR_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_REMOTE_HOST, PR_JOT_LOGFMT_REMOTE_HOST_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_REMOTE_IP, PR_JOT_LOGFMT_REMOTE_IP_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_IDENT_USER, PR_JOT_LOGFMT_IDENT_USER_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_PID, PR_JOT_LOGFMT_PID_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_TIME, PR_JOT_LOGFMT_TIME_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_SECONDS, PR_JOT_LOGFMT_SECONDS_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_COMMAND, PR_JOT_LOGFMT_COMMAND_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_LOCAL_NAME, PR_JOT_LOGFMT_LOCAL_NAME_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_LOCAL_PORT, PR_JOT_LOGFMT_LOCAL_PORT_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_LOCAL_IP, PR_JOT_LOGFMT_LOCAL_IP_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_LOCAL_FQDN, PR_JOT_LOGFMT_LOCAL_FQDN_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_USER, PR_JOT_LOGFMT_USER_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_ORIGINAL_USER, PR_JOT_LOGFMT_ORIG_USER_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_RESPONSE_CODE,
    PR_JOT_LOGFMT_RESPONSE_CODE_KEY, PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_CLASS, PR_JOT_LOGFMT_CLASS_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_ANON_PASS, PR_JOT_LOGFMT_ANON_PASSWD_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_METHOD, PR_JOT_LOGFMT_METHOD_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_XFER_PATH, PR_JOT_LOGFMT_XFER_PATH_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_DIR_NAME, PR_JOT_LOGFMT_DIR_NAME_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_DIR_PATH, PR_JOT_LOGFMT_DIR_PATH_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_CMD_PARAMS, PR_JOT_LOGFMT_CMD_PARAMS_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_RESPONSE_STR,
    PR_JOT_LOGFMT_RESPONSE_MSG_KEY, PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_PROTOCOL, PR_JOT_LOGFMT_PROTOCOL_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_VERSION, PR_JOT_LOGFMT_VERSION_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_RENAME_FROM, PR_JOT_LOGFMT_RENAME_FROM_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_FILE_MODIFIED,
    PR_JOT_LOGFMT_FILE_MODIFIED_KEY, PR_JSON_TYPE_BOOL);
  add_json_info(p, map, LOGFMT_META_UID, PR_JOT_LOGFMT_UID_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_GID, PR_JOT_LOGFMT_GID_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_RAW_BYTES_IN,
    PR_JOT_LOGFMT_RAW_BYTES_IN_KEY, PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_RAW_BYTES_OUT,
    PR_JOT_LOGFMT_RAW_BYTES_OUT_KEY, PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_EOS_REASON, PR_JOT_LOGFMT_EOS_REASON_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_VHOST_IP, PR_JOT_LOGFMT_VHOST_IP_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_NOTE_VAR, PR_JOT_LOGFMT_NOTE_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_XFER_STATUS, PR_JOT_LOGFMT_XFER_STATUS_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_XFER_FAILURE,
    PR_JOT_LOGFMT_XFER_FAILURE_KEY, PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_MICROSECS, PR_JOT_LOGFMT_MICROSECS_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_MILLISECS, PR_JOT_LOGFMT_MILLISECS_KEY,
    PR_JSON_TYPE_NUMBER);
  add_json_info(p, map, LOGFMT_META_ISO8601, PR_JOT_LOGFMT_ISO8601_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_GROUP, PR_JOT_LOGFMT_GROUP_KEY,
    PR_JSON_TYPE_STRING);
  add_json_info(p, map, LOGFMT_META_CONNECT, PR_JOT_LOGFMT_CONNECT_KEY,
    PR_JSON_TYPE_BOOL);
  add_json_info(p, map, LOGFMT_META_DISCONNECT, PR_JOT_LOGFMT_DISCONNECT_KEY,
    PR_JSON_TYPE_BOOL);

  return map;
}

void pr_jot_on_json(pool *p, pr_jot_ctx_t *ctx, unsigned char logfmt_id,
    const char *jot_key, const void *val) {
  const struct logfmt_json_info *lji;
  pr_json_object_t *json;
  pr_table_t *logfmt_json_map;

  if (p == NULL ||
      ctx == NULL ||
      val == NULL) {
    return;
  }

  if (ctx->log == NULL) {
    pr_trace_msg(trace_channel, 16,
      "missing required JSON object for jotting LogFormat ID %u",
      (unsigned int) logfmt_id);
    return;
  }

  if (ctx->user_data == NULL) {
    pr_trace_msg(trace_channel, 16,
      "missing required JSON map for jotting LogFormat ID %u",
      (unsigned int) logfmt_id);
    return;
  }

  json = ctx->log;
  logfmt_json_map = (pr_table_t *) ctx->user_data;

  lji = pr_table_kget(logfmt_json_map, (const void *) &logfmt_id,
    sizeof(unsigned char), NULL);
  if (lji == NULL) {
    pr_trace_msg(trace_channel, 16,
      "missing required JSON information for jotting LogFormat ID %u",
      (unsigned int) logfmt_id);
    return;
  }

  pr_trace_msg(trace_channel, 18, "jotting LogFormat ID %u as JSON %s (%s)",
    (unsigned int) logfmt_id, pr_json_type_name(lji->json_type), lji->json_key);

  switch (lji->json_type) {
    case PR_JSON_TYPE_STRING: {
      const char *json_key;

      json_key = lji->json_key;

      /* Use the hinted key, if available (e.g. for ENV/NOTE variables). */
      if (jot_key != NULL) {
        json_key = jot_key;
      }

      (void) pr_json_object_set_string(p, json, json_key, (const char *) val);
      break;
    }

    case PR_JSON_TYPE_NUMBER:
      (void) pr_json_object_set_number(p, json, lji->json_key,
        *((double *) val));
      break;

    case PR_JSON_TYPE_BOOL:
      (void) pr_json_object_set_bool(p, json, lji->json_key, *((int *) val));
      break;
  }
}

static char *get_meta_arg(pool *p, unsigned char *meta, size_t *arg_len) {
  char buf[PR_TUNABLE_PATH_MAX+1], *ptr;
  size_t len;

  ptr = buf;
  len = 0;

  while (*meta != LOGFMT_META_ARG_END) {
    pr_signals_handle();
    *ptr++ = (char) *meta++;
    len++;
  }

  *ptr = '\0';
  *arg_len = len;

  return pstrdup(p, buf);
}

static const char *get_meta_dir_name(cmd_rec *cmd) {
  const char *dir_name = NULL;
  pool *p;

  p = cmd->tmp_pool;

  if (pr_cmd_cmp(cmd, PR_CMD_CDUP_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_CWD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_LIST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MLSD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MKD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_NLST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_RMD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XCWD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XCUP_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XMKD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XRMD_ID) == 0) {
    char *path, *ptr;

    path = pr_fs_decode_path(p, cmd->arg);
    ptr = strrchr(path, '/');

    if (ptr != NULL) {
      if (ptr != path) {
        dir_name = ptr + 1;

      } else if (*(ptr + 1) != '\0') {
        dir_name = ptr + 1;

      } else {
        dir_name = path;
      }

    } else {
      dir_name = path;
    }

  } else {
    dir_name = pr_fs_getvwd();
  }

  return dir_name;
}

static const char *get_meta_dir_path(cmd_rec *cmd) {
  const char *dir_path = NULL;
  pool *p;

  p = cmd->tmp_pool;

  if (pr_cmd_cmp(cmd, PR_CMD_CDUP_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_LIST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MLSD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MKD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_NLST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_RMD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XCUP_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XMKD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_XRMD_ID) == 0) {
    dir_path = dir_abs_path(p, pr_fs_decode_path(p, cmd->arg), TRUE);

  } else if (pr_cmd_cmp(cmd, PR_CMD_CWD_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_XCWD_ID) == 0) {

    /* Note: by this point in the dispatch cycle, the current working
     * directory has already been changed.  For the CWD/XCWD commands, this
     * means that dir_abs_path() may return an improper path, with the target
     * directory being reported twice.  To deal with this, do not use
     * dir_abs_path(), and use pr_fs_getvwd()/pr_fs_getcwd() instead.
     */

    if (session.chroot_path != NULL) {
      /* Chrooted session. */
      if (strncmp(pr_fs_getvwd(), "/", 2) == 0) {
        dir_path = session.chroot_path;

      } else {
        dir_path = pdircat(p, session.chroot_path, pr_fs_getvwd(), NULL);
      }

    } else {
      /* Non-chrooted session. */
      dir_path = pr_fs_getcwd();
    }
  }

  return dir_path;
}

static const char *get_meta_filename(cmd_rec *cmd) {
  const char *filename = NULL;
  pool *p;

  p = cmd->tmp_pool;

  if (pr_cmd_cmp(cmd, PR_CMD_RNTO_ID) == 0) {
    filename = dir_abs_path(p, pr_fs_decode_path(p, cmd->arg), TRUE);

  } else if (pr_cmd_cmp(cmd, PR_CMD_RETR_ID) == 0) {
    const char *path;

    path = pr_table_get(cmd->notes, "mod_xfer.retr-path", NULL);
    if (path != NULL) {
      filename = dir_abs_path(p, path, TRUE);
    }

  } else if (pr_cmd_cmp(cmd, PR_CMD_APPE_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_STOR_ID) == 0) {
    const char *path;

    path = pr_table_get(cmd->notes, "mod_xfer.store-path", NULL);
    if (path != NULL) {
      filename = dir_abs_path(p, path, TRUE);
    }

  } else if (session.xfer.p != NULL &&
             session.xfer.path != NULL) {
    filename = dir_abs_path(p, session.xfer.path, TRUE);

  } else if (pr_cmd_cmp(cmd, PR_CMD_CDUP_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_PWD_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_XCUP_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_XPWD_ID) == 0) {
    filename = dir_abs_path(p, pr_fs_getcwd(), TRUE);

  } else if (pr_cmd_cmp(cmd, PR_CMD_CWD_ID) == 0 ||
             pr_cmd_cmp(cmd, PR_CMD_XCWD_ID) == 0) {

    /* Note: by this point in the dispatch cycle, the current working
     * directory has already been changed.  For the CWD/XCWD commands, this
     * means that dir_abs_path() may return an improper path, with the target
     * directory being reported twice.  To deal with this, do not use
     * dir_abs_path(), and use pr_fs_getvwd()/pr_fs_getcwd() instead.
     */
    if (session.chroot_path != NULL) {
      /* Chrooted session. */
      if (strncmp(pr_fs_getvwd(), "/", 2) == 0) {
        filename = session.chroot_path;

      } else {
        filename = pdircat(p, session.chroot_path, pr_fs_getvwd(), NULL);
      }

    } else {
      /* Non-chrooted session. */
      filename = pr_fs_getcwd();
    }

  } else if (pr_cmd_cmp(cmd, PR_CMD_SITE_ID) == 0 &&
             (strncasecmp(cmd->argv[1], "CHGRP", 6) == 0 ||
              strncasecmp(cmd->argv[1], "CHMOD", 6) == 0 ||
              strncasecmp(cmd->argv[1], "UTIME", 6) == 0)) {
    register unsigned int i;
    char *ptr = "";

    for (i = 3; i <= cmd->argc-1; i++) {
      ptr = pstrcat(p, ptr, *ptr ? " " : "",
        pr_fs_decode_path(p, cmd->argv[i]), NULL);
    }

    filename = dir_abs_path(p, ptr, TRUE);

  } else {
    /* Some commands (i.e. DELE, MKD, RMD, XMKD, and XRMD) have associated
     * filenames that are not stored in the session.xfer structure; these
     * should be expanded properly as well.
     */
    if (pr_cmd_cmp(cmd, PR_CMD_DELE_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_LIST_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_MDTM_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_MKD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_MLSD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_MLST_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_NLST_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_RMD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_XMKD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_XRMD_ID) == 0) {
      filename = dir_abs_path(p, pr_fs_decode_path(p, cmd->arg), TRUE);

    } else if (pr_cmd_cmp(cmd, PR_CMD_MFMT_ID) == 0) {
      /* MFMT has, as its filename, the second argument. */
      filename = dir_abs_path(p, pr_fs_decode_path(p, cmd->argv[2]), TRUE);
    }
  }

  return filename;
}

static const char *get_meta_transfer_failure(cmd_rec *cmd) {
  const char *transfer_failure = NULL;

  /* If the current command is one that incurs a data transfer, then we
   * need to do more work.  If not, it's an easy substitution.
   */
  if (pr_cmd_cmp(cmd, PR_CMD_APPE_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_LIST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MLSD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_NLST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_RETR_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_STOR_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_STOU_ID) == 0) {
    const char *proto;

    proto = pr_session_get_protocol(0);

    if (strncmp(proto, "ftp", 4) == 0 ||
        strncmp(proto, "ftps", 5) == 0) {

      if (!(XFER_ABORTED)) {
        int res;
        const char *resp_code = NULL, *resp_msg = NULL;

        /* Get the last response code/message.  We use heuristics here to
         * determine when to use "failed" versus "success".
         */
        res = pr_response_get_last(cmd->tmp_pool, &resp_code, &resp_msg);
        if (res == 0 &&
            resp_code != NULL) {
          if (*resp_code != '2' &&
              *resp_code != '1') {
            char *ptr;

            /* Parse out/prettify the resp_msg here */
            ptr = strchr(resp_msg, '.');
            if (ptr != NULL) {
              transfer_failure = ptr + 2;

            } else {
              transfer_failure = resp_msg;
            }
          }
        }
      }
    }
  }

  return transfer_failure;
}

static const char *get_meta_transfer_path(cmd_rec *cmd) {
  const char *transfer_path = NULL;

  if (pr_cmd_cmp(cmd, PR_CMD_RNTO_ID) == 0) {
    transfer_path = dir_best_path(cmd->tmp_pool,
      pr_fs_decode_path(cmd->tmp_pool, cmd->arg));

  } else if (session.xfer.p != NULL &&
             session.xfer.path != NULL) {
    transfer_path = session.xfer.path;

  } else {
    /* Some commands (i.e. DELE, MKD, XMKD, RMD, XRMD) have associated
     * filenames that are not stored in the session.xfer structure; these
     * should be expanded properly as well.
     */
    if (pr_cmd_cmp(cmd, PR_CMD_DELE_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_MKD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_XMKD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_RMD_ID) == 0 ||
        pr_cmd_cmp(cmd, PR_CMD_XRMD_ID) == 0) {
      transfer_path = dir_best_path(cmd->tmp_pool,
        pr_fs_decode_path(cmd->tmp_pool, cmd->arg));
    }
  }

  return transfer_path;
}

static int get_meta_transfer_secs(cmd_rec *cmd, double *transfer_secs) {
  if (session.xfer.p == NULL) {
    return -1;
  }

  /* Make sure that session.xfer.start_time actually has values (which is
   * not always the case).
   */
  if (session.xfer.start_time.tv_sec != 0 ||
      session.xfer.start_time.tv_usec != 0) {
    struct timeval end_time;

    gettimeofday(&end_time, NULL);
    end_time.tv_sec -= session.xfer.start_time.tv_sec;

    if (end_time.tv_usec >= session.xfer.start_time.tv_usec) {
      end_time.tv_usec -= session.xfer.start_time.tv_usec;

    } else {
      end_time.tv_usec = 1000000L - (session.xfer.start_time.tv_usec -
        end_time.tv_usec);
      end_time.tv_sec--;
    }

    *transfer_secs = end_time.tv_sec;
    *transfer_secs += (double) ((double) end_time.tv_usec / (double) 1000);

    return 0;
  }

  return -1;
}

static const char *get_meta_transfer_status(cmd_rec *cmd) {
  const char *transfer_status = NULL;

  /* If the current command is one that incurs a data transfer, then we need
   * to do more work.  If not, it's an easy substitution.
   */
  if (pr_cmd_cmp(cmd, PR_CMD_ABOR_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_APPE_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_LIST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_MLSD_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_NLST_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_RETR_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_STOR_ID) == 0 ||
      pr_cmd_cmp(cmd, PR_CMD_STOU_ID) == 0) {
    const char *proto;

    proto = pr_session_get_protocol(0);

    if (strncmp(proto, "ftp", 4) == 0 ||
        strncmp(proto, "ftps", 5) == 0) {
      if (!(XFER_ABORTED)) {
        int res;
        const char *resp_code = NULL, *resp_msg = NULL;

        /* Get the last response code/message.  We use heuristics here to
         * determine when to use "failed" versus "success".
         */
        res = pr_response_get_last(cmd->tmp_pool, &resp_code, &resp_msg);
        if (res == 0 &&
            resp_code != NULL) {
          if (*resp_code == '2') {
            if (pr_cmd_cmp(cmd, PR_CMD_ABOR_ID) != 0) {
              transfer_status = "success";

            } else {
              /* We're handling the ABOR command, so obviously the value
               * should be 'cancelled'.
               */
              transfer_status = "cancelled";
            }

          } else if (*resp_code == '1') {

            /* If the first digit of the response code is 1, then the
             * response code (for a data transfer command) is probably 150,
             * which means that the transfer was still in progress (didn't
             * complete with a 2xx/4xx response code) when we are called here,
             * which in turn means a timeout kicked in.
             */

            transfer_status = "timeout";

          } else {
            transfer_status = "failed";
          }

        } else {
          transfer_status = "success";
        }

      } else {
        transfer_status = "cancelled";
      }

    } else {
      /* mod_sftp stashes a note for us in the command notes if the transfer
       * failed.
       */
      const char *sftp_status;

      sftp_status = pr_table_get(cmd->notes, "mod_sftp.file-status", NULL);
      if (sftp_status == NULL) {
        transfer_status = "success";

      } else {
        transfer_status = "failed";
      }
    }
  }

  return transfer_status;
}

static void resolve_meta(pool *p, unsigned char **logfmt, pr_jot_ctx_t *ctx,
    cmd_rec *cmd, void (*on_meta)(pool *, pr_jot_ctx_t *, unsigned char,
      const char *, const void *)) {
  unsigned char *ptr, logfmt_id;
  int auto_adjust_ptr = TRUE;

  ptr = (*logfmt) + 1;
  logfmt_id = *ptr;

  switch (logfmt_id) {
    case LOGFMT_META_BYTES_SENT: {
      double bytes_sent;
      int have_bytes = FALSE;

      if (session.xfer.p != NULL) {
        bytes_sent = session.xfer.total_bytes;
        have_bytes = TRUE;

      } else if (pr_cmd_cmp(cmd, PR_CMD_DELE_ID) == 0) {
        bytes_sent = jot_deleted_filesz;
        have_bytes = TRUE;
      }

      if (have_bytes == TRUE) {
        (on_meta)(p, ctx, logfmt_id, NULL, &bytes_sent);
      }

      break;
    }

    case LOGFMT_META_FILENAME: {
      const char *filename;

      filename = get_meta_filename(cmd);
      if (filename != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, filename);
      }

      break;
    }

    case LOGFMT_META_ENV_VAR: {
      ptr++;

      if (*ptr == LOGFMT_META_START &&
          *(ptr + 1) == LOGFMT_META_ARG) {
        char *key, *env = NULL;
        size_t key_len = 0;

        key = get_meta_arg(p, (ptr + 2), &key_len);
        ptr += key_len;

        env = pr_env_get(p, key);
        if (env != NULL) {
          char *field_name;

          field_name = pstrcat(p, PR_JOT_LOGFMT_ENV_VAR_KEY, key, NULL);
          (on_meta)(p, ctx, logfmt_id, field_name, env);
        }
      }

      auto_adjust_ptr = FALSE;
      break;
    }

    case LOGFMT_META_REMOTE_HOST: {
      const char *name;

      name = pr_netaddr_get_sess_remote_name();
      (on_meta)(p, ctx, logfmt_id, NULL, name);
      break;
    }

    case LOGFMT_META_REMOTE_IP: {
      const char *ipstr;

      ipstr = pr_netaddr_get_ipstr(pr_netaddr_get_sess_local_addr());
      (on_meta)(p, ctx, logfmt_id, NULL, ipstr);
      break;
    }

    case LOGFMT_META_IDENT_USER: {
      const char *ident_user;

      ident_user = pr_table_get(session.notes, "mod_ident.rfc1413-ident", NULL);
      if (ident_user != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, ident_user);
      }

      break;
    }

    case LOGFMT_META_PID: {
      double sess_pid;

      sess_pid = session.pid;
      (on_meta)(p, ctx, logfmt_id, NULL, &sess_pid);
      break;
    }

    case LOGFMT_META_TIME: {
      char ts[128], *time_fmt = "%Y-%m-%d %H:%M:%S %z";
      struct tm *tm;
      time_t now;

      ptr++;

      now = time(NULL);
      tm = pr_gmtime(NULL, &now);

      if (*ptr == LOGFMT_META_START &&
          *(ptr + 1) == LOGFMT_META_ARG) {
        size_t fmt_len = 0;

        time_fmt = get_meta_arg(p, (ptr + 2), &fmt_len);
      }

      strftime(ts, sizeof(ts)-1, time_fmt, tm);
      (on_meta)(p, ctx, logfmt_id, NULL, ts);

      auto_adjust_ptr = FALSE;
      break;
    }

    case LOGFMT_META_SECONDS: {
      double transfer_secs;

      if (get_meta_transfer_secs(cmd, &transfer_secs) == 0) {
        (on_meta)(p, ctx, logfmt_id, NULL, &transfer_secs);
      }

      break;
    }

    case LOGFMT_META_COMMAND: {
      const char *full_cmd;

      /* Note: Ignore "fake" commands like CONNECT, DISCONNECT, EXIT. */
      if ((cmd->cmd_class & CL_CONNECT) ||
          (cmd->cmd_class & CL_DISCONNECT)) {
        full_cmd = NULL;

      } else {
        if (pr_cmd_cmp(cmd, PR_CMD_PASS_ID) == 0 &&
            session.hide_password) {
          full_cmd = "PASS (hidden)";

        } else if (pr_cmd_cmp(cmd, PR_CMD_ADAT_ID) == 0) {
          full_cmd = "ADAT (hidden)";

        } else {
          full_cmd = get_full_cmd(cmd);
        }
      }

      if (full_cmd != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, full_cmd);
      }

      break;
    }

    case LOGFMT_META_LOCAL_NAME: {
      (on_meta)(p, ctx, logfmt_id, NULL, cmd->server->ServerName);
      break;
    }

    case LOGFMT_META_LOCAL_PORT: {
      double server_port;

      server_port = cmd->server->ServerPort;
      (on_meta)(p, ctx, logfmt_id, NULL, &server_port);
      break;
    }

    case LOGFMT_META_LOCAL_IP: {
      const char *ipstr;

      ipstr = pr_netaddr_get_ipstr(pr_netaddr_get_sess_local_addr());
      (on_meta)(p, ctx, logfmt_id, NULL, ipstr);
      break;
    }

    case LOGFMT_META_LOCAL_FQDN: {
      const char *dnsstr;

      dnsstr = pr_netaddr_get_dnsstr(pr_netaddr_get_sess_local_addr());
      (on_meta)(p, ctx, logfmt_id, NULL, dnsstr);
      break;
    }

    case LOGFMT_META_USER: {
      if (session.user != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, session.user);
      }

      break;
    }

    case LOGFMT_META_ORIGINAL_USER: {
      const char *orig_user = NULL;

      orig_user = pr_table_get(session.notes, "mod_auth.orig-user", NULL);
      if (orig_user != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, orig_user);
      }

      break;
    }

    case LOGFMT_META_RESPONSE_CODE: {
      const char *resp_code = NULL;
      double resp_num;
      int res;

      res = pr_response_get_last(cmd->tmp_pool, &resp_code, NULL);
      if (res == 0 &&
          resp_code != NULL) {
        resp_num = atoi(resp_code);

      /* Hack to add return code for proper logging of QUIT command. */
      } else if (pr_cmd_cmp(cmd, PR_CMD_QUIT_ID) == 0) {
        res = 0;
        resp_num = 221;
      }

      if (res == 0) {
        (on_meta)(p, ctx, logfmt_id, NULL, &resp_num);
      }

      break;
    }

    case LOGFMT_META_CLASS: {
      if (session.conn_class != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, session.conn_class);
      }

      break;
    }

    case LOGFMT_META_ANON_PASS: {
      const char *anon_pass;

      anon_pass = pr_table_get(session.notes, "mod_auth.anon-passwd", NULL);
      if (anon_pass == NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, anon_pass);
      }

      break;
    }

    case LOGFMT_META_METHOD: {
      const char *method = NULL;

      if (pr_cmd_cmp(cmd, PR_CMD_SITE_ID) != 0) {
        /* Note: Ignore "fake" commands like CONNECT, DISCONNECT, EXIT. */
        if (!(cmd->cmd_class & CL_CONNECT) &&
            !(cmd->cmd_class & CL_DISCONNECT)) {
          method = cmd->argv[0];
        }

      } else {
        char buf[128], *ch;

        /* Make sure that the SITE command used is all in uppercase, for
         * logging purposes.
         */
        for (ch = cmd->argv[1]; *ch; ch++) {
          *ch = toupper((int) *ch);
        }

        memset(buf, '\0', sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "%s %s", (char *) cmd->argv[0],
          (char *) cmd->argv[1]);

        method = pstrdup(p, buf);
      }

      if (method != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, method);
      }

      break;
    }

    case LOGFMT_META_XFER_PATH: {
      const char *transfer_path;

      transfer_path = get_meta_transfer_path(cmd);
      if (transfer_path != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, transfer_path);
      }

      break;
    }

    case LOGFMT_META_DIR_NAME: {
      const char *dir_name;

      dir_name = get_meta_dir_name(cmd);
      if (dir_name != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, dir_name);
      }

      break;
    }

    case LOGFMT_META_DIR_PATH: {
      const char *dir_path;

      dir_path = get_meta_dir_path(cmd);
      if (dir_path != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, dir_path);
      }

      break;
    }

    case LOGFMT_META_CMD_PARAMS: {
      const char *params = NULL;

      if (pr_cmd_cmp(cmd, PR_CMD_ADAT_ID) == 0 ||
          pr_cmd_cmp(cmd, PR_CMD_PASS_ID) == 0) {
        params = "(hidden)";

      } else if (cmd->argc > 1) {
        params = pr_fs_decode_path(p, cmd->arg);
      }

      if (params != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, params);
      }

      break;
    }

    case LOGFMT_META_RESPONSE_STR: {
      const char *resp_msg = NULL;
      int res;

      res = pr_response_get_last(p, NULL, &resp_msg);
      if (res == 0 &&
          resp_msg != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, resp_msg);
      }

      break;
    }

    case LOGFMT_META_PROTOCOL: {
      const char *proto;

      proto = pr_session_get_protocol(0);
      (on_meta)(p, ctx, logfmt_id, NULL, proto);
      break;
    }

    case LOGFMT_META_VERSION: {
      const char *version;

      version = PROFTPD_VERSION_TEXT;
      (on_meta)(p, ctx, logfmt_id, NULL, version);
      break;
    }

    case LOGFMT_META_RENAME_FROM: {
      if (pr_cmd_cmp(cmd, PR_CMD_RNTO_ID) == 0) {
        const char *rnfr_path;

        rnfr_path = pr_table_get(session.notes, "mod_core.rnfr-path", NULL);
        if (rnfr_path != NULL) {
          (on_meta)(p, ctx, logfmt_id, NULL, rnfr_path);
        }
      }

      break;
    }

    case LOGFMT_META_FILE_MODIFIED: {
      int modified = FALSE;
      const char *val;

      val = pr_table_get(cmd->notes, "mod_xfer.file-modified", NULL);
      if (val != NULL) {
        if (strncmp(val, "true", 5) == 0) {
          modified = TRUE;
        }
      }

      (on_meta)(p, ctx, logfmt_id, NULL, &modified);
      break;
    }

    case LOGFMT_META_UID: {
      double sess_uid;

      sess_uid = session.login_uid;
      (on_meta)(p, ctx, logfmt_id, NULL, &sess_uid);
      break;
    }

    case LOGFMT_META_GID: {
      double sess_gid;

      sess_gid = session.login_gid;
      (on_meta)(p, ctx, logfmt_id, NULL, &sess_gid);
      break;
    }

    case LOGFMT_META_RAW_BYTES_IN: {
      double bytes_rcvd;

      bytes_rcvd = session.total_raw_in;
      (on_meta)(p, ctx, logfmt_id, NULL, &bytes_rcvd);
      break;
    }

    case LOGFMT_META_RAW_BYTES_OUT: {
      double bytes_sent;

      bytes_sent = session.total_raw_out;
      (on_meta)(p, ctx, logfmt_id, NULL, &bytes_sent);
      break;
    }

    case LOGFMT_META_EOS_REASON: {
      const char *reason = NULL, *details = NULL, *eos = NULL;

      eos = pr_session_get_disconnect_reason(&details);
      if (eos != NULL) {
        if (details != NULL) {
          reason = pstrcat(p, eos, ": ", details, NULL);

        } else {
          reason = eos;
        }
      }

      if (reason != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, reason);
      }

      break;
    }

    case LOGFMT_META_VHOST_IP:
      (on_meta)(p, ctx, logfmt_id, NULL, cmd->server->ServerAddress);
      break;

    case LOGFMT_META_NOTE_VAR: {
      ptr++;

      if (*ptr == LOGFMT_META_START &&
          *(ptr + 1) == LOGFMT_META_ARG) {
        const char *note = NULL;
        char *key;
        size_t key_len = 0;

        key = get_meta_arg(p, (ptr + 2), &key_len);
        ptr += key_len;

        /* Check in the cmd->notes table first. */
        note = pr_table_get(cmd->notes, key, NULL);
        if (note == NULL) {
          /* If not there, check in the session.notes table. */
          note = pr_table_get(session.notes, key, NULL);
        }

        if (note != NULL) {
          char *field_name;

          field_name = pstrcat(p, PR_JOT_LOGFMT_NOTE_KEY, note, NULL);
          (on_meta)(p, ctx, logfmt_id, field_name, note);
        }
      }

      auto_adjust_ptr = FALSE;
      break;
    }

    case LOGFMT_META_XFER_STATUS: {
      const char *transfer_status;

      transfer_status = get_meta_transfer_status(cmd);
      if (transfer_status != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, transfer_status);
      }

      break;
    }

    case LOGFMT_META_XFER_FAILURE: {
      const char *transfer_failure;

      transfer_failure = get_meta_transfer_failure(cmd);
      if (transfer_failure != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, transfer_failure);
      }

      break;
    }

    case LOGFMT_META_MICROSECS: {
      double sess_usecs;
      struct timeval now;

      gettimeofday(&now, NULL);
      sess_usecs = now.tv_usec;

      (on_meta)(p, ctx, logfmt_id, NULL, &sess_usecs);
      break;
    }

    case LOGFMT_META_MILLISECS: {
      double sess_msecs;
      struct timeval now;

      gettimeofday(&now, NULL);

      /* Convert microsecs to millisecs. */
      sess_msecs = (now.tv_usec / 1000);

      (on_meta)(p, ctx, logfmt_id, NULL, &sess_msecs);
      break;
    }

    case LOGFMT_META_ISO8601: {
      char ts[128];
      struct tm *tm;
      struct timeval now;
      unsigned long millis;
      size_t len;

      gettimeofday(&now, NULL);
      tm = pr_localtime(NULL, (const time_t *) &(now.tv_sec));

      len = strftime(ts, sizeof(ts)-1, "%Y-%m-%d %H:%M:%S", tm);

      /* Convert microsecs to millisecs. */
      millis = now.tv_usec / 1000;

      snprintf(ts + len, sizeof(ts) - len - 1, ",%03lu", millis);
      (on_meta)(p, ctx, logfmt_id, NULL, ts);
      break;
    }

    case LOGFMT_META_GROUP: {
      if (session.group != NULL) {
        (on_meta)(p, ctx, logfmt_id, NULL, session.group);
      }

      break;
    }

    default:
      pr_trace_msg(trace_channel, 2, "skipping unsupported LogFormat ID %u",
        (unsigned int) logfmt_id);
      break;
  }

  /* Most of the time, a meta is encoded in just one byte, so we adjust the
   * pointer by incrementing by one.  Some meta are encoded using multiple
   * bytes (e.g. environment variables, notes, etc).  The resolving of these
   * meta will adjust the pointer as needed themselves, in which case they
   * set adjust_ptr = false.
   */

  if (auto_adjust_ptr == TRUE) {
    ptr++;
  }

  *logfmt = ptr;
}

static int is_jottable_class(cmd_rec *cmd, int included_classes,
    int excluded_classes) {
  int jottable = FALSE;

  if (cmd->cmd_class & included_classes) {
    jottable = TRUE;
  }

  if (cmd->cmd_class & excluded_classes) {
    jottable = FALSE;
  }

  /* If the logging class of this command is unknown (defaults to zero),
   * AND this filter logs ALL events, it is jottable.
   */
  if (cmd->cmd_class == 0 &&
      included_classes == CL_ALL) {
    jottable = TRUE;
  }

  return jottable;
}

static int is_jottable_cmd(cmd_rec *cmd, int *cmd_ids, size_t ncmd_ids) {
  register unsigned int i;
  int jottable = FALSE;

  for (i = 0; i < ncmd_ids; i++) {
    if (pr_cmd_cmp(cmd, cmd_ids[i]) == 0) {
      jottable = TRUE;
      break;
    }
  }
 
  return jottable;
}

static int is_jottable(pool *p, cmd_rec *cmd, pr_jot_filters_t *filters) {
  int jottable = FALSE;

  if (filters == NULL) {
    return TRUE;
  }

  jottable = is_jottable_class(cmd, filters->included_classes,
    filters->excluded_classes);
  if (jottable == TRUE) {
    return TRUE;
  }

  jottable = is_jottable_cmd(cmd, filters->cmd_ids->elts,
    filters->cmd_ids->nelts);
  return jottable;
}

int pr_jot_resolve_logfmt(pool *p, cmd_rec *cmd, pr_jot_filters_t *filters,
    unsigned char *logfmt, pr_jot_ctx_t *ctx,
    void (*on_meta)(pool *, pr_jot_ctx_t *, unsigned char, const char *,
      const void *),
    void (*on_other)(pool *, pr_jot_ctx_t *, unsigned char)) {
  int jottable = FALSE;

  if (p == NULL ||
      cmd == NULL ||
      logfmt == NULL ||
      on_meta == NULL) {
    errno = EINVAL;
    return -1;
  }

  jottable = is_jottable(p, cmd, filters);
  if (jottable == FALSE) {
    pr_trace_msg(trace_channel, 17, "ignoring filtered event '%s'",
      (const char *) cmd->argv[0]);
    return 0;
  }

  /* Special handling for the CONNECT/DISCONNECT meta. */
  if (cmd->cmd_class == CL_CONNECT) {
    int val = TRUE;
    (on_meta)(p, ctx, LOGFMT_META_CONNECT, NULL, &val);

  } else if (cmd->cmd_class == CL_DISCONNECT) {
    int val = TRUE;
    (on_meta)(p, ctx, LOGFMT_META_DISCONNECT, NULL, &val);
  }

  while (*logfmt) {
    pr_signals_handle();

    if (*logfmt == LOGFMT_META_START) {
      resolve_meta(p, &logfmt, ctx, cmd, on_meta);

    } else {
      if (on_other != NULL) {
        (on_other)(p, ctx, *logfmt);
      }

      logfmt++;
    }
  }

  return 0;
}

/* XXX Would be nice to a pr_str_csv2array() function */
static array_header *filter_csv2array(pool *p, char *csv) {
  array_header *names;
  char *ptr, *name;

  names = make_array(p, 1, sizeof(char *));

  ptr = csv;
  name = pr_str_get_word(&ptr, 0);
  while (name != NULL) {
    pr_signals_handle();

    *((char **) push_array(names)) = pstrdup(p, name);

    /* Skip commas and pipes. */
    while (*ptr == ',' ||
           *ptr == '|') {
      ptr++;
    }

    name = pr_str_get_word(&ptr, 0);
  }

  return names;
}

static int filter_get_classes(pool *p, array_header *names,
    int *included_classes, int *excluded_classes) {
  register unsigned int i;
  int incl, excl, exclude = FALSE;

  incl = excl = CL_NONE;

  for (i = 0; i < names->nelts; i++) {
    const char *name;

    pr_signals_handle();

    name = ((const char **) names->elts)[i];

    if (*name == '!') {
      exclude = TRUE;
      name++;
    }

    if (strcasecmp(name, "NONE") == 0) {
      if (exclude) {
        incl = CL_ALL;
        excl = CL_NONE;

      } else {
        incl = CL_NONE;
      }

    } else if (strcasecmp(name, "ALL") == 0) {
      if (exclude) {
        incl = CL_NONE;
        excl = CL_ALL;

      } else {
        incl = CL_ALL;
      }

    } else if (strcasecmp(name, "AUTH") == 0) {
      if (exclude) {
        incl &= ~CL_AUTH;
        excl |= CL_AUTH;

      } else {
        incl |= CL_AUTH;
      }

    } else if (strcasecmp(name, "INFO") == 0) {
      if (exclude) {
        incl &= ~CL_INFO;
        excl |= CL_INFO;

      } else {
        incl |= CL_INFO;
      }

    } else if (strcasecmp(name, "DIRS") == 0) {
      if (exclude) {
        incl &= ~CL_DIRS;
        excl |= CL_DIRS;

      } else {
        incl |= CL_DIRS;
      }

    } else if (strcasecmp(name, "READ") == 0) {
      if (exclude) {
        incl &= ~CL_READ;
        excl |= CL_READ;

      } else {
        incl |= CL_READ;
      }

    } else if (strcasecmp(name, "WRITE") == 0) {
      if (exclude) {
        incl &= ~CL_WRITE;
        excl |= CL_WRITE;

      } else {
        incl |= CL_WRITE;
      }

    } else if (strcasecmp(name, "MISC") == 0) {
      if (exclude) {
        incl &= ~CL_MISC;
        excl |= CL_MISC;

      } else {
        incl |= CL_MISC;
      }

    } else if (strcasecmp(name, "SEC") == 0 ||
               strcasecmp(name, "SECURE") == 0) {
      if (exclude) {
        incl &= ~CL_SEC;
        excl |= CL_SEC;

      } else {
        incl |= CL_SEC;
      }

    } else if (strcasecmp(name, "CONNECT") == 0) {
      if (exclude) {
        incl &= ~CL_CONNECT;
        excl |= CL_CONNECT;

      } else {
        incl |= CL_CONNECT;
      }

    } else if (strcasecmp(name, "EXIT") == 0 ||
               strcasecmp(name, "DISCONNECT") == 0) {
      if (exclude) {
        incl &= ~CL_DISCONNECT;
        excl |= CL_DISCONNECT;

      } else {
        incl |= CL_DISCONNECT;
      }

    } else if (strcasecmp(name, "SSH") == 0) {
      if (exclude) {
        incl &= ~CL_SSH;
        excl |= CL_SSH;

      } else {
        incl |= CL_SSH;
      }

    } else if (strcasecmp(name, "SFTP") == 0) {
      if (exclude) {
        incl &= ~CL_SFTP;
        excl |= CL_SFTP;

      } else {
        incl |= CL_SFTP;
      }

    } else {
      pr_trace_msg(trace_channel, 2, "ignoring unknown/unsupported class '%s'",
        name);
      errno = ENOENT;
      return -1;
    }
  }

  *included_classes = incl;
  *excluded_classes = excl;
  return 0;
}

static array_header *filter_get_cmd_ids(pool *p, array_header *names,
    int *included_classes, int *excluded_classes, int rules_type, int flags) {
  register unsigned int i;
  array_header *cmd_ids;

  cmd_ids = make_array(p, names->nelts, sizeof(int));
  for (i = 0; i < names->nelts; i++) {
    const char *name;
    int cmd_id, valid = TRUE;

    pr_signals_handle();

    name = ((const char **) names->elts)[i];

    cmd_id = pr_cmd_get_id(name);
    if (cmd_id < 0) {
      valid = FALSE;

      if (rules_type == PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES) {
        if (strcmp(name, "ALL") == 0) {
          *included_classes = CL_ALL;
          valid = TRUE;

          if (flags & PR_JOT_FILTER_FL_ALL_INCL_ALL) {
            *included_classes |= (CL_CONNECT|CL_DISCONNECT);
          }

        } else if (strcmp(name, "CONNECT") == 0) {
          *included_classes |= CL_CONNECT;
          valid = TRUE;

        } else if (strcmp(name, "DISCONNECT") == 0) {
          *included_classes |= CL_DISCONNECT;
          valid = TRUE;
        } 
      }

      if (valid == FALSE) {
        pr_trace_msg(trace_channel, 2, "ignoring unknown command '%s'", name);
      }
    }

    if (valid == TRUE) {
      *((int *) push_array(cmd_ids)) = cmd_id;
    }
  }

  return cmd_ids;
}

pr_jot_filters_t *pr_jot_filters_create(pool *p, const char *rules,
    int rules_type, int flags) {
  int included_classes, excluded_classes;
  pool *sub_pool, *tmp_pool;
  array_header *cmd_ids, *names;
  pr_jot_filters_t *filters;

  if (p == NULL ||
      rules == NULL) {
    errno = EINVAL;
    return NULL;
  }

  included_classes = excluded_classes = CL_NONE;
  cmd_ids = NULL;

  sub_pool = make_sub_pool(p);
  pr_pool_tag(sub_pool, "Jot Filters pool");

  tmp_pool = make_sub_pool(p);
  names = filter_csv2array(tmp_pool, pstrdup(tmp_pool, rules));

  switch (rules_type) {
    case PR_JOT_FILTER_TYPE_CLASSES: {
      int res;

      res = filter_get_classes(sub_pool, names, &included_classes,
        &excluded_classes);
      if (res < 0) {
        int xerrno = errno;

        destroy_pool(tmp_pool);
        destroy_pool(sub_pool);
        errno = xerrno;
        return NULL;
      }

      break;
    }

    case PR_JOT_FILTER_TYPE_COMMANDS:
    case PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES:
      cmd_ids = filter_get_cmd_ids(sub_pool, names, &included_classes,
        &excluded_classes, rules_type, flags);
      break;

    default:
      destroy_pool(tmp_pool);
      destroy_pool(sub_pool);
      errno = EINVAL;
      return NULL;
  }

  destroy_pool(tmp_pool);

  filters = pcalloc(sub_pool, sizeof(pr_jot_filters_t));
  filters->pool = sub_pool;
  filters->included_classes = included_classes;
  filters->excluded_classes = excluded_classes;
  filters->cmd_ids = cmd_ids;

  return filters;
}

int pr_jot_filters_destroy(pr_jot_filters_t *filters) {
  if (filters == NULL) {
    errno = EINVAL;
    return -1;
  }

  destroy_pool(filters->pool);
  return 0;
}

void jot_set_deleted_filesz(off_t deleted_filesz) {
  jot_deleted_filesz = deleted_filesz;
}
