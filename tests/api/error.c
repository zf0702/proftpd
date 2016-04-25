/*
 * ProFTPD - FTP server testsuite
 * Copyright (c) 2016 The ProFTPD Project team
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

/* Error API tests */

#include "tests.h"
#include "error.h"

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("error", 1, 20);
  }
}

static void tear_down(void) {
  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("error", 0, 0);
  }

  if (p) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  }
}

static const char *get_errnum(pool *err_pool, int xerrno) {
  char errnum[32];
  memset(errnum, '\0', sizeof(errnum));
  snprintf(errnum, sizeof(errnum)-1, "%d", xerrno);
  return pstrdup(err_pool, errnum);
}

START_TEST (error_create_test) {
  pr_error_t *err;

  err = pr_error_create(NULL, 0);
  fail_unless(err == NULL, "Failed handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  err = pr_error_create(p, -1);
  fail_unless(err == NULL, "Failed handle negative errno");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  err = pr_error_create(p, 0);
  fail_unless(err != NULL, "Failed allocate error: %s", strerror(errno));
  pr_error_destroy(err);
}
END_TEST

START_TEST (error_destroy_test) {
  pr_error_t *err;
  int xerrno = 77;

  err = pr_error_create(p, 0);
  fail_unless(err != NULL, "Failed allocate error: %s", strerror(errno));

  /* Make sure that pr_error_destroy() preserves the existing errno value. */
  errno = xerrno;
  pr_error_destroy(NULL);
  pr_error_destroy(err);

  fail_unless(errno == xerrno, "Expected errno %d, got %d", xerrno, errno);
}
END_TEST

START_TEST (error_set_goal_test) {
  int res;
  pr_error_t *err;

  res = pr_error_set_goal(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  err = pr_error_create(p, 1);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_set_goal(err, NULL);
  fail_unless(res < 0, "Failed to handle null goal");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_error_set_goal(err, "because I wanted to");
  fail_unless(res == 0, "Failed to set goal: %s", strerror(errno));

  pr_error_destroy(err);
}
END_TEST

START_TEST (error_set_location_test) {
  int res;
  pr_error_t *err;

  res = pr_error_set_location(NULL, NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  err = pr_error_create(p, 1);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_set_location(err, NULL, NULL, 0);
  fail_unless(res == 0, "Failed to set location: %s", strerror(errno));

  pr_error_destroy(err);
}
END_TEST

START_TEST (error_set_operation_test) {
  int res;
  pr_error_t *err;

  res = pr_error_set_operation(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  err = pr_error_create(p, 1);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_set_operation(err, NULL);
  fail_unless(res < 0, "Failed to handle null operation");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_error_set_operation(err, "testing");
  fail_unless(res == 0, "Failed to set operation: %s", strerror(errno));

  pr_error_destroy(err);
}
END_TEST

START_TEST (error_explanations_test) {
  module m;
  const char *name;
  pr_error_explanations_t *explanations;
  int res;

  /* Unregister with none registered -- ENOENT */

  res = pr_error_unregister_explanations(NULL, NULL, NULL);
  fail_unless(res < 0, "Failed to handle null arguments");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  name = "testing";
  res = pr_error_unregister_explanations(p, NULL, name);
  fail_unless(res < 0, "Failed to handle no registered explanations");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  memset(&m, 0, sizeof(m));
  m.name = "error";

  res = pr_error_unregister_explanations(p, &m, NULL);
  fail_unless(res < 0, "Failed to handle no registered explanations");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_error_unregister_explanations(p, &m, name);
  fail_unless(res < 0, "Failed to handle no registered explanations");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_error_use_explanations(p, NULL, NULL);
  fail_unless(res < 0, "Failed to handle no registered explanations");
  fail_unless(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  explanations = pr_error_register_explanations(NULL, NULL, NULL);
  fail_unless(explanations == NULL, "Failed to handle null pool argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  explanations = pr_error_register_explanations(p, NULL, NULL);
  fail_unless(explanations == NULL, "Failed to handle null name argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  explanations = pr_error_register_explanations(p, &m, name);
  fail_unless(explanations != NULL, "Failed to register '%s' explanations: %s",
    name, strerror(errno));

  explanations = pr_error_register_explanations(p, &m, name);
  fail_unless(explanations == NULL, "Failed to handle duplicate registration");
  fail_unless(errno == EEXIST, "Expected EEXIST (%d), got %s (%d)", EEXIST,
    strerror(errno), errno);

  res = pr_error_unregister_explanations(p, &m, name);
  fail_unless(res == 0, "Failed to handle unregister '%s' explanations: %s",
    name, strerror(errno));

  res = pr_error_unregister_explanations(p, &m, name);
  fail_unless(res < 0, "Failed to handle no registered explanations");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  /* Wildcard unregistrations: ANY_MODULE, null name. */

  explanations = pr_error_register_explanations(p, &m, name);
  fail_unless(explanations != NULL, "Failed to register '%s' explanations: %s",
    name, strerror(errno));

  res = pr_error_unregister_explanations(p, ANY_MODULE, name);
  fail_unless(res == 0, "Failed to handle unregister '%s' explanations: %s",
    name, strerror(errno));

  explanations = pr_error_register_explanations(p, &m, name);
  fail_unless(explanations != NULL, "Failed to register '%s' explanations: %s",
    name, strerror(errno));

  res = pr_error_unregister_explanations(p, &m, NULL);
  fail_unless(res == 0, "Failed to handle unregister module explanations: %s",
    strerror(errno));

  /* Selecting the explanations to use. */

  explanations = pr_error_register_explanations(p, &m, name);
  fail_unless(explanations != NULL, "Failed to register '%s' explanations: %s",
    name, strerror(errno));

  res = pr_error_use_explanations(p, &m, NULL);
  fail_unless(res < 0, "Failed to handle null name argument");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_error_use_explanations(p, &m, "foobar");
  fail_unless(res < 0, "Used 'foobar' explanations unexpectedly");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_error_use_explanations(p, &m, name);
  fail_unless(res == 0, "Failed to use '%s' explanations: %s", name,
    strerror(errno));

  /* Use already-selected explanations */
  res = pr_error_use_explanations(p, &m, name);
  fail_unless(res == 0, "Failed to use '%s' explanations: %s", name,
    strerror(errno));

  res = pr_error_unregister_explanations(p, &m, name);
  fail_unless(res == 0, "Failed to handle unregister module explanations: %s",
    strerror(errno));
}
END_TEST

START_TEST (error_strerror_minimal_test) {
  int format = PR_ERROR_FORMAT_USE_MINIMAL, xerrno;
  pr_error_t *err;
  const char *res, *expected, *oper;

  xerrno = errno = ENOENT;
  expected = strerror(xerrno);
  res = pr_error_strerror(NULL, format);
  fail_unless(res != NULL, "Failed to handle null error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_strerror(err, -1);
  fail_unless(res != NULL, "Failed to handle invalid format: %s",
    strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  expected = pstrcat(p, "No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);
  xerrno = 0;

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  expected = "Success (EOK [0])";
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);

  /* We want to test what happens when we use an invalid errno value. */
  xerrno = INT_MAX - 786;

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  expected = pstrcat(p, strerror(xerrno), " (<unknown/unsupported error> [",
    get_errnum(p, xerrno), "])", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);
  xerrno = ENOSYS;

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  oper = "test";
  pr_error_set_operation(err, oper);

  expected = pstrcat(p, oper, " failed with \"", strerror(xerrno), " (ENOSYS [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  oper = "test2";
  pr_error_set_operation(err, oper);

  expected = pstrcat(p, oper, " failed with \"", strerror(xerrno),
    " (ENOSYS [", get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);
}
END_TEST

START_TEST (error_strerror_terse_test) {
  int format = PR_ERROR_FORMAT_USE_TERSE, xerrno;
  pr_error_t *err;
  const char *res, *expected, *oper;

  xerrno = errno = ENOENT;
  expected = strerror(xerrno);
  res = pr_error_strerror(NULL, format);
  fail_unless(res != NULL, "Failed to handle null error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_strerror(err, -1);
  fail_unless(res != NULL, "Failed to handle invalid format: %s",
    strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  expected = pstrcat(p, "No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  oper = "test2";
  pr_error_set_operation(err, oper);

  expected = pstrcat(p, oper, " failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);
}
END_TEST

START_TEST (error_strerror_detailed_test) {
  int format = PR_ERROR_FORMAT_USE_DETAILED, xerrno, res2, error_details;
  pr_error_t *err;
  const char *res, *expected, *oper, *goal;

  xerrno = errno = ENOENT;
  expected = strerror(xerrno);
  res = pr_error_strerror(NULL, format);
  fail_unless(res != NULL, "Failed to handle null error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  res = pr_error_strerror(err, -1);
  fail_unless(res != NULL, "Failed to handle invalid format: %s",
    strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* no oper */
  expected = pstrcat(p,
    "in core failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  res2 = pr_error_set_location(err, NULL, __FILE__, __LINE__);
  fail_unless(res2 == 0, "Failed to set error location: %s", strerror(errno));

  expected = pstrcat(p,
    "in core [api/error.c:443] failed with \"No such file or directory "
    "(ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Disable use of the module name. */
  error_details = pr_error_use_details(PR_ERROR_DETAILS_DEFAULT);
  error_details &= ~PR_ERROR_DETAILS_USE_MODULE;
  (void) pr_error_use_details(error_details);

  expected = pstrcat(p,
    "in api/error.c:443 failed with \"No such file or directory "
    "(ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Disable use of the file location. */
  error_details &= ~PR_ERROR_DETAILS_USE_FILE;
  (void) pr_error_use_details(error_details);

  /* We have no user/group, no location, no goal, no operation.  Expect the
   * default/fallback, then.
   */
  expected = strerror(xerrno);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  oper = "test";
  res2 = pr_error_set_operation(err, oper);
  fail_unless(res2 == 0, "Failed to set operation '%s': %s", oper,
    strerror(errno));

  expected = pstrcat(p, oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  session.user = "foo";
  session.uid = 77;
  session.group = "bar";
  session.gid = 88;

  expected = pstrcat(p, "user ", session.user, " (UID 77)/group ",
    session.group, " (GID 88) via ftp attempted ", oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Disable use of names. */
  error_details |= (PR_ERROR_DETAILS_USE_MODULE|PR_ERROR_DETAILS_USE_FILE);
  error_details &= ~PR_ERROR_DETAILS_USE_NAMES;
  (void) pr_error_use_details(error_details);

  expected = pstrcat(p, "UID 77/GID 88 via ftp in core [api/error.c:443] "
    "attempted ", oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Enable use of names, disable use of IDs. */
  error_details |= PR_ERROR_DETAILS_USE_NAMES;
  error_details &= ~PR_ERROR_DETAILS_USE_IDS;
  (void) pr_error_use_details(error_details);

  expected = pstrcat(p, "user ", session.user, "/group ", session.group,
    " via ftp in core [api/error.c:443] attempted ", oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Enable use of IDs, disable use of protocol. */
  error_details |= PR_ERROR_DETAILS_USE_IDS;
  error_details &= ~PR_ERROR_DETAILS_USE_PROTOCOL;
  (void) pr_error_use_details(error_details);

  expected = pstrcat(p, "user ", session.user, " (UID 77)/group ",
    session.group, " (GID 88) in core [api/error.c:443] attempted ", oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, format);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  /* Enable everything */
  error_details = PR_ERROR_DETAILS_DEFAULT;
  (void) pr_error_use_details(error_details);

  goal = "test a function";
  res2 = pr_error_set_goal(err, goal);
  fail_unless(res2 == 0, "Failed to set goal: %s", strerror(errno));

  expected = pstrcat(p, "user ", session.user, " (UID 77)/group ",
    session.group, " (GID 88) via ftp wanted to ", goal,
    " in core [api/error.c:443] but ", oper,
    " failed with \"No such file or directory (ENOENT [",
    get_errnum(p, xerrno), "])\"", NULL);
  res = pr_error_strerror(err, 0);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  pr_error_destroy(err);
  pr_error_use_details(PR_ERROR_DETAILS_DEFAULT);
}
END_TEST

static const char *test_explain_open(pool *err_pool, int xerrno,
    const char *path, int flags, mode_t mode, const char **args) {
  *args = pstrcat(err_pool, "path = '", path,
    "', flags = O_RDONLY, mode = 0755", NULL);
  return pstrdup(err_pool, "test mode is not real");
}

START_TEST (error_strerror_detailed_explained_test) {
  int xerrno, res2;
  pr_error_t *err;
  pr_error_explanations_t *explainers;
  const char *res, *expected, *oper, *goal;
  module m;

  xerrno = ENOENT;
  err = pr_error_create(p, xerrno);
  fail_unless(err != NULL, "Failed to allocate error: %s", strerror(errno));

  oper = "test";
  res2 = pr_error_set_operation(err, oper);
  fail_unless(res2 == 0, "Failed to set operation: %s", strerror(errno));

  goal = "demonstrate an error explanation";
  res2 = pr_error_set_goal(err, goal);
  fail_unless(res2 == 0, "Failed to set goal: %s", strerror(errno));

  memset(&m, 0, sizeof(m));
  m.name = "error";

  session.user = "foo";
  session.uid = 7;
  session.group = "bar";
  session.gid = 8;

  res2 = pr_error_set_location(err, &m, __FILE__, __LINE__);
  fail_unless(res2 == 0, "Failed to set location: %s", strerror(errno));

  explainers = pr_error_register_explanations(p, &m, "error");
  explainers->explain_open = test_explain_open;

  res2 = pr_error_explain_open(err, "path", O_RDONLY, 0755);
  fail_unless(res2 == 0, "Failed to explain error: %s", strerror(errno));

  expected = pstrcat(p, "user ", session.user, " (UID 7)/group ",
    session.group, " (GID 8) via ftp wanted to ", goal, " in mod_",
    m.name, " [api/error.c:606] but open() using path = 'path', "
    "flags = O_RDONLY, mode = 0755 failed with \"No such file or directory ("
    "ENOENT [", get_errnum(p, xerrno), "])\" because test mode is not real",
    NULL);
  res = pr_error_strerror(err, 0);
  fail_unless(res != NULL, "Failed to format error: %s", strerror(errno));
  fail_unless(strcmp(res, expected) == 0, "Expected '%s', got '%s'", expected,
    res);

  (void) pr_error_unregister_explanations(p, &m, NULL);
  pr_error_destroy(err);
}

END_TEST

Suite *tests_get_error_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("error");
  testcase = tcase_create("base");

  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, error_create_test);
  tcase_add_test(testcase, error_destroy_test);
  tcase_add_test(testcase, error_set_goal_test);
  tcase_add_test(testcase, error_set_location_test);
  tcase_add_test(testcase, error_set_operation_test);
  tcase_add_test(testcase, error_explanations_test);
  tcase_add_test(testcase, error_strerror_minimal_test);
  tcase_add_test(testcase, error_strerror_terse_test);
  tcase_add_test(testcase, error_strerror_detailed_test);
  tcase_add_test(testcase, error_strerror_detailed_explained_test);

#if 0

/* These tests need to cover with and without explainers.  And with explainers,
 * test null/non-null explanations
 */
  tcase_add_test(testcase, error_explain_accept_test);
  tcase_add_test(testcase, error_explain_bind_test);
  tcase_add_test(testcase, error_explain_chdir_test);
  tcase_add_test(testcase, error_explain_chmod_test);
  tcase_add_test(testcase, error_explain_chown_test);
  tcase_add_test(testcase, error_explain_chroot_test);
  tcase_add_test(testcase, error_explain_close_test);
  tcase_add_test(testcase, error_explain_closedir_test);
  tcase_add_test(testcase, error_explain_connect_test);
  tcase_add_test(testcase, error_explain_fchmod_test);
  tcase_add_test(testcase, error_explain_fchown_test);
  tcase_add_test(testcase, error_explain_fclose_test);
  tcase_add_test(testcase, error_explain_fcntl_test);
  tcase_add_test(testcase, error_explain_fdopen_test);
  tcase_add_test(testcase, error_explain_flock_test);
  tcase_add_test(testcase, error_explain_fopen_test);
  tcase_add_test(testcase, error_explain_fork_test);
  tcase_add_test(testcase, error_explain_fstat_test);
  tcase_add_test(testcase, error_explain_fstatfs_test);
  tcase_add_test(testcase, error_explain_fstatvfs_test);
  tcase_add_test(testcase, error_explain_fsync_test);
  tcase_add_test(testcase, error_explain_ftruncate_test);
  tcase_add_test(testcase, error_explain_futimes_test);
  tcase_add_test(testcase, error_explain_getaddrinfo_test);
  tcase_add_test(testcase, error_explain_getcwd_test);
  tcase_add_test(testcase, error_explain_gethostbyname_test);
  tcase_add_test(testcase, error_explain_gethostbyname2_test);
  tcase_add_test(testcase, error_explain_gethostname_test);
  tcase_add_test(testcase, error_explain_getnameinfo_test);
  tcase_add_test(testcase, error_explain_getpeername_test);
  tcase_add_test(testcase, error_explain_getrlimit_test);
  tcase_add_test(testcase, error_explain_getsockname_test);
  tcase_add_test(testcase, error_explain_getsockopt_test);
  tcase_add_test(testcase, error_explain_gettimeofday_test);
  tcase_add_test(testcase, error_explain_lchmod_test);
  tcase_add_test(testcase, error_explain_lchown_test);
  tcase_add_test(testcase, error_explain_link_test);
  tcase_add_test(testcase, error_explain_listen_test);
  tcase_add_test(testcase, error_explain_lseek_test);
  tcase_add_test(testcase, error_explain_lstat_test);
  tcase_add_test(testcase, error_explain_lutimes_test);
  tcase_add_test(testcase, error_explain_mkdir_test);
  tcase_add_test(testcase, error_explain_mkdtemp_test);
  tcase_add_test(testcase, error_explain_mkstemp_test);
  tcase_add_test(testcase, error_explain_open_test);
  tcase_add_test(testcase, error_explain_opendir_test);
  tcase_add_test(testcase, error_explain_read_test);
  tcase_add_test(testcase, error_explain_readdir_test);
  tcase_add_test(testcase, error_explain_readlink_test);
  tcase_add_test(testcase, error_explain_readv_test);
  tcase_add_test(testcase, error_explain_rename_test);
  tcase_add_test(testcase, error_explain_rmdir_test);
  tcase_add_test(testcase, error_explain_setegid_test);
  tcase_add_test(testcase, error_explain_seteuid_test);
  tcase_add_test(testcase, error_explain_setgid_test);
  tcase_add_test(testcase, error_explain_setregid_test);
  tcase_add_test(testcase, error_explain_setresgid_test);
  tcase_add_test(testcase, error_explain_setresuid_test);
  tcase_add_test(testcase, error_explain_setreuid_test);
  tcase_add_test(testcase, error_explain_setrlimit_test);
  tcase_add_test(testcase, error_explain_setsockopt_test);
  tcase_add_test(testcase, error_explain_setuid_test);
  tcase_add_test(testcase, error_explain_socket_test);
  tcase_add_test(testcase, error_explain_stat_test);
  tcase_add_test(testcase, error_explain_statfs_test);
  tcase_add_test(testcase, error_explain_statvfs_test);
  tcase_add_test(testcase, error_explain_symlink_test);
  tcase_add_test(testcase, error_explain_truncate_test);
  tcase_add_test(testcase, error_explain_unlink_test);
  tcase_add_test(testcase, error_explain_utimes_test);
  tcase_add_test(testcase, error_explain_write_test);
  tcase_add_test(testcase, error_explain_writev_test);
#endif

  suite_add_tcase(suite, testcase);
  return suite;
}
