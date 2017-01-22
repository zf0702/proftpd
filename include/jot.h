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
 * As a special exemption, The ProFTPD Project team and other respective
 * copyright holders give permission to link this program with OpenSSL, and
 * distribute the resulting executable, without including the source code for
 * OpenSSL in the source distribution.
 */

/* Jot API */

#ifndef PR_JOT_H
#define PR_JOT_H

#include "conf.h"

/* Jot keys for LogFormat variables, e.g. for key/value logging via JSON. */
#define PR_JOT_LOGFMT_ANON_PASSWD_KEY	"anon_password"
#define PR_JOT_LOGFMT_BYTES_SENT_KEY	"bytes_sent"
#define PR_JOT_LOGFMT_CLASS_KEY		"connection_class"
#define PR_JOT_LOGFMT_COMMAND_KEY	"raw_command"
#define PR_JOT_LOGFMT_CONNECT_KEY	"connecting"
#define PR_JOT_LOGFMT_CMD_PARAMS_KEY	"command_params"
#define PR_JOT_LOGFMT_DIR_NAME_KEY	"dir_name"
#define PR_JOT_LOGFMT_DIR_PATH_KEY	"dir_path"
#define PR_JOT_LOGFMT_DISCONNECT_KEY	"disconnecting"
#define PR_JOT_LOGFMT_ENV_VAR_KEY	"ENV:"
#define PR_JOT_LOGFMT_EOS_REASON_KEY	"session_end_reason"
#define PR_JOT_LOGFMT_FILENAME_KEY	"file"
#define PR_JOT_LOGFMT_FILE_MODIFIED_KEY	"file_modified"
#define PR_JOT_LOGFMT_GID_KEY		"gid"
#define PR_JOT_LOGFMT_GROUP_KEY		"group"
#define PR_JOT_LOGFMT_IDENT_USER_KEY	"identd_user"
#define PR_JOT_LOGFMT_ISO8601_KEY	"timestamp"
#define PR_JOT_LOGFMT_LOCAL_FQDN_KEY	"server_dns"
#define PR_JOT_LOGFMT_LOCAL_IP_KEY	"local_ip"
#define PR_JOT_LOGFMT_LOCAL_NAME_KEY	"server_name"
#define PR_JOT_LOGFMT_LOCAL_PORT_KEY	"local_port"
#define PR_JOT_LOGFMT_METHOD_KEY	"command"
#define PR_JOT_LOGFMT_MILLISECS_KEY	"millisecs"
#define PR_JOT_LOGFMT_MICROSECS_KEY	"microsecs"
#define PR_JOT_LOGFMT_NOTE_KEY		"NOTE:"
#define PR_JOT_LOGFMT_ORIG_USER_KEY	"original_user"
#define PR_JOT_LOGFMT_PID_KEY		"pid"
#define PR_JOT_LOGFMT_PROTOCOL_KEY	"protocol"
#define PR_JOT_LOGFMT_RAW_BYTES_IN_KEY	"session_bytes_rcvd"
#define PR_JOT_LOGFMT_RAW_BYTES_OUT_KEY	"session_bytes_sent"
#define PR_JOT_LOGFMT_REMOTE_HOST_KEY	"remote_dns"
#define PR_JOT_LOGFMT_REMOTE_IP_KEY	"remote_ip"
#define PR_JOT_LOGFMT_RENAME_FROM_KEY	"rename_from"
#define PR_JOT_LOGFMT_RESPONSE_CODE_KEY	"response_code"
#define PR_JOT_LOGFMT_RESPONSE_MSG_KEY	"response_msg"
#define PR_JOT_LOGFMT_SECONDS_KEY	"transfer_secs"
#define PR_JOT_LOGFMT_TIME_KEY		"local_time"
#define PR_JOT_LOGFMT_UID_KEY		"uid"
#define PR_JOT_LOGFMT_USER_KEY		"user"
#define PR_JOT_LOGFMT_VERSION_KEY	"server_version"
#define PR_JOT_LOGFMT_VHOST_IP_KEY	"server_ip"
#define PR_JOT_LOGFMT_XFER_PATH_KEY	"transfer_path"
#define PR_JOT_LOGFMT_XFER_FAILURE_KEY	"transfer_failure"
#define PR_JOT_LOGFMT_XFER_STATUS_KEY	"transfer_status"

/* This opaque structure is used for tracking filters for events. */
typedef struct jot_filters_rec pr_jot_filters_t;

/* Use this for passing data to your jotting callbacks. */
typedef struct {
  /* A pointer to the object into which resolved variables are written. */
  void *log;

  /* User-supplied data/context to use when writing resolved variables. */
  const void *user_data;

} pr_jot_ctx_t;

/* Returns table which maps LOGFMT_META_ values  to JSON keys and types. */
pr_table_t *pr_jot_get_logfmt2json(pool *p);

pr_jot_filters_t *pr_jot_filters_create(pool *p, const char *rules,
  int rules_type, int flags);
#define PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES	0
#define PR_JOT_FILTER_TYPE_COMMANDS			1
#define PR_JOT_FILTER_TYPE_CLASSES			2

/* Use this flag to indicate that an "ALL" name means _everything_.  By
 * default, the CL_ALL logging class does NOT include all classes, due to
 * backward compatibility requirements.
 */
#define PR_JOT_FILTER_FL_ALL_INCL_ALL		0x001

int pr_jot_filters_destroy(pr_jot_filters_t *filters);

int pr_jot_resolve_logfmt(pool *p, cmd_rec *cmd, pr_jot_filters_t *filters,
  unsigned char *logfmt, pr_jot_ctx_t *ctx,
  void (*on_meta)(pool *, pr_jot_ctx_t *, unsigned char, const char *,
    const void *),
  void (*on_other)(pool *, pr_jot_ctx_t *, unsigned char));

/* Canned `on_meta` callback to use when resolving LogFormat strings into
 * JSON objects.
 */
void pr_jot_on_json(pool *p, pr_jot_ctx_t *ctx, unsigned char logfmt_id,
  const char *key, const void *val);

/* For internal use only. */
void jot_set_deleted_filesz(off_t deleted_filesz);

#endif /* PR_JOT_H */
