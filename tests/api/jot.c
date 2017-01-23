/*
 * ProFTPD - FTP server testsuite
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

/* Jot API tests. */

#include "tests.h"
#include "logfmt.h"
#include "json.h"
#include "jot.h"

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = make_sub_pool(NULL);
  }

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("jot", 1, 20);
  }
}

static void tear_down(void) {
  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("jot", 0, 0);
  }

  if (p) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  }
}

/* Tests */

static void assert_jot_class_filter(const char *class_name) {
  pr_jot_filters_t *filters;
  const char *rules;

  rules = class_name;

  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  rules = pstrcat(p, "!", class_name, NULL);

  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);
}

START_TEST (jot_filters_create_test) {
  pr_jot_filters_t *filters;
  const char *rules;

  mark_point();
  filters = pr_jot_filters_create(NULL, NULL, 0, 0);
  fail_unless(filters == NULL, "Failed to handle null pool");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  filters = pr_jot_filters_create(p, NULL, 0, 0);
  fail_unless(filters == NULL, "Failed to handle null rules");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  rules = "foo";

  mark_point();
  filters = pr_jot_filters_create(p, rules, -1, 0);
  fail_unless(filters == NULL, "Failed to handle invalid rules type");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  /* Class rules */

  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_CLASSES, 0);
  fail_unless(filters == NULL, "Failed to handle invalid class name '%s'",
    rules);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  assert_jot_class_filter("NONE");
  assert_jot_class_filter("ALL");
  assert_jot_class_filter("AUTH");
  assert_jot_class_filter("INFO");
  assert_jot_class_filter("DIRS");
  assert_jot_class_filter("READ");
  assert_jot_class_filter("WRITE");
  assert_jot_class_filter("SEC");
  assert_jot_class_filter("SECURE");
  assert_jot_class_filter("CONNECT");
  assert_jot_class_filter("EXIT");
  assert_jot_class_filter("DISCONNECT");
  assert_jot_class_filter("SSH");
  assert_jot_class_filter("SFTP");

  rules = "AUTH,!INFO";

  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  rules = "!INFO|AUTH";

  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  /* Command rules */

  rules = "FOO,BAR";
  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_COMMANDS, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  rules = "APPE,RETR,STOR,STOU";
  mark_point();
  filters = pr_jot_filters_create(p, rules, PR_JOT_FILTER_TYPE_COMMANDS, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  /* Rules with commands and classes */

  rules = "CONNECT,RETR,STOR,DISCONNECT";
  mark_point();
  filters = pr_jot_filters_create(p, rules,
    PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  rules = "ALL";
  mark_point();
  filters = pr_jot_filters_create(p, rules,
    PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES, 0);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);

  /* Flags */

  rules = "ALL";
  mark_point();
  filters = pr_jot_filters_create(p, rules,
    PR_JOT_FILTER_TYPE_COMMANDS_WITH_CLASSES, PR_JOT_FILTER_FL_ALL_INCL_ALL);
  fail_unless(filters != NULL, "Failed to create filters from '%s': %s",
    rules, strerror(errno));
  (void) pr_jot_filters_destroy(filters);
}
END_TEST

START_TEST (jot_filters_destroy_test) {
  int res;
  pr_jot_filters_t *filters;

  mark_point();
  res = pr_jot_filters_destroy(NULL);
  fail_unless(res < 0, "Failed to handle null filters");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  filters = pr_jot_filters_create(p, "NONE", PR_JOT_FILTER_TYPE_CLASSES, 0);

  mark_point();
  res = pr_jot_filters_destroy(filters);
  fail_unless(res == 0, "Failed to destroy filters: %s", strerror(errno));
}
END_TEST

START_TEST (jot_filters_include_classes_test) {
  int res;
  pr_jot_filters_t *filters;

  mark_point();
  res = pr_jot_filters_include_classes(NULL, 0);
  fail_unless(res < 0, "Failed to handle null filters");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  filters = pr_jot_filters_create(p, "NONE", PR_JOT_FILTER_TYPE_CLASSES, 0);

  res = pr_jot_filters_include_classes(filters, CL_ALL);
  fail_unless(res == FALSE, "Expected FALSE, got %d", res);

  res = pr_jot_filters_include_classes(filters, CL_NONE);
  fail_unless(res == TRUE, "Expected TRUE, got %d", res);

  res = pr_jot_filters_destroy(filters);
  fail_unless(res == 0, "Failed to destroy filters: %s", strerror(errno));
}
END_TEST

START_TEST (jot_resolve_logfmt_id_test) {
  /* XXX TODO */
}
END_TEST

START_TEST (jot_resolve_logfmt_test) {
  /* XXX TODO */
}
END_TEST

START_TEST (jot_on_json_test) {
  pr_jot_ctx_t *ctx;
  pr_json_object_t *json;
  double num;
  int truth;
  const char *text;

  mark_point();
  pr_jot_on_json(NULL, NULL, 0, NULL, NULL);

  mark_point();
  pr_jot_on_json(p, NULL, 0, NULL, NULL);

  ctx = pcalloc(p, sizeof(pr_jot_ctx_t));

  mark_point();
  pr_jot_on_json(p, ctx, 0, NULL, NULL);

  mark_point();
  pr_jot_on_json(p, ctx, 0, NULL, &num);

  json = pr_json_object_alloc(p);
  ctx->log = json;

  mark_point();
  pr_jot_on_json(p, ctx, 0, NULL, &num);

  ctx->user_data = pr_table_alloc(p, 0);

  mark_point();
  pr_jot_on_json(p, ctx, 0, NULL, &num);

  ctx->user_data = pr_jot_get_logfmt2json(p);

  mark_point();
  truth = FALSE;
  pr_jot_on_json(p, ctx, LOGFMT_META_CONNECT, NULL, &truth);

  mark_point();
  num = 2476;
  pr_jot_on_json(p, ctx, LOGFMT_META_PID, NULL, &num);

  mark_point();
  text = "lorem ipsum";
  pr_jot_on_json(p, ctx, LOGFMT_META_IDENT_USER, NULL, text);

  mark_point();
  text = "alef bet vet";
  pr_jot_on_json(p, ctx, LOGFMT_META_USER, "USER_KEY", text);

  (void) pr_json_object_free(json);
}
END_TEST

START_TEST (jot_get_logfmt2json_test) {
  pr_table_t *res;

  mark_point();
  res = pr_jot_get_logfmt2json(NULL);
  fail_unless(res == NULL, "Failed to handle null pool");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = pr_jot_get_logfmt2json(p);
  fail_unless(res != NULL, "Failed to get map: %s", strerror(errno));
}
END_TEST

Suite *tests_get_jot_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("jot");
  testcase = tcase_create("base");

  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, jot_filters_create_test);
  tcase_add_test(testcase, jot_filters_destroy_test);
  tcase_add_test(testcase, jot_filters_include_classes_test);
  tcase_add_test(testcase, jot_resolve_logfmt_id_test);
  tcase_add_test(testcase, jot_resolve_logfmt_test);
  tcase_add_test(testcase, jot_on_json_test);
  tcase_add_test(testcase, jot_get_logfmt2json_test);

  suite_add_tcase(suite, testcase);
  return suite;
}
