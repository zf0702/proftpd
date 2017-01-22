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

/* Jot Types */

#define PR_JOT_TYPE_STRING		1
#define PR_JOT_TYPE_NUMBER		2
#define PR_JOT_TYPE_BOOL		3

/* Jot Names for LogFormat variables, e.g. for key/value logging. */
#define PR_JOT_LOGFMT_ENV_VAR_NAME	"ENV:"
#define PR_JOT_LOGFMT_NOTE_NAME		"NOTE:"

/* This opaque structure is used for tracking filters for events. */
typedef struct jot_filters_rec pr_jot_filters_t;

/* Use this for passing data to your jotting callbacks. */
typedef struct {
  /* A pointer to the object into which resolved variables are written. */
  void *log;

  /* User-supplied data/context to use when writing resolved variables. */
  const void *user_data;

} pr_jot_ctx_t;

/* XXX */
const char *pr_jot_type_name(unsigned int jot_type);

/* Returns table which maps LOGFMT IDs to JSON keys and types. */
pr_table_t *pr_jot_get_logfmt2json_map(pool *p);

pr_jot_filters_t *pr_jot_filters_create(pool *p, const char *rules, int opts,
  int flags);
#define PR_JOT_FILTER_OPT_COMMANDS_WITH_CLASSES	0
#define PR_JOT_FILTER_OPT_COMMANDS		1
#define PR_JOT_FILTER_OPT_CLASSES		2

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
