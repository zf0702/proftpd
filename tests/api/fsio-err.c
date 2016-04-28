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

/* FSIO with error tests. */

#include "tests.h"
#include "fsio-err.h"

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  init_fs();
  pr_fs_statcache_set_policy(PR_TUNABLE_FS_STATCACHE_SIZE,
    PR_TUNABLE_FS_STATCACHE_MAX_AGE, 0);

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("error", 1, 20);
    pr_trace_set_levels("fsio", 1, 20);
  }
}

static void tear_down(void) {
  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("error", 0, 0);
    pr_trace_set_levels("fsio", 0, 0);
  }

  pr_fs_statcache_set_policy(PR_TUNABLE_FS_STATCACHE_SIZE,
    PR_TUNABLE_FS_STATCACHE_MAX_AGE, 0);

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

START_TEST (fsio_open_with_error_test) {
  int flags, xerrno;
  const char *expected, *errstr, *path;
  pr_fh_t *fh;
  pr_error_t *err = NULL;

  flags = O_RDONLY;
  path = "/no/such/path";

  fh = pr_fsio_open_with_error(NULL, path, flags, NULL);
  fail_unless(fh == NULL, "Unexpectedly opened '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open_with_error(p, path, flags, NULL);
  fail_unless(fh == NULL, "Unexpectedly opened '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  fh = pr_fsio_open_with_error(NULL, path, flags, &err);
  fail_unless(fh == NULL, "Unexpectedly opened '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  fh = pr_fsio_open_with_error(p, path, flags, &err);
  fail_unless(fh == NULL, "Unexpectedly opened '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "open() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_close_with_error_test) {
  int res, xerrno;
  const char *expected, *errstr;
  pr_error_t *err = NULL;

  res = pr_fsio_close_with_error(NULL, NULL, NULL);
  fail_unless(res < 0, "Unexpectedly closed null handle");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_close_with_error(p, NULL, NULL);
  fail_unless(res < 0, "Unexpectedly closed null handle");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = pr_fsio_close_with_error(NULL, NULL, &err);
  fail_unless(res < 0, "Unexpectedly closed null handle");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error");

  mark_point();
  res = pr_fsio_close_with_error(p, NULL, &err);
  fail_unless(res < 0, "Unexpectedly closed null handle");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "close() failed with \"", strerror(xerrno),
    " (EINVAL [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_read_with_error_test) {
  int res, xerrno;
  pr_fh_t *fh;
  char *buf;
  size_t sz;
  const char *expected, *errstr;
  pr_error_t *err = NULL;

  fh = NULL;
  buf = NULL;
  sz = 0;

  res = pr_fsio_read_with_error(NULL, fh, buf, sz, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with read");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_read_with_error(p, fh, buf, sz, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with read");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_read_with_error(NULL, fh, buf, sz, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with read");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_read_with_error(p, fh, buf, sz, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with read");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "read() failed with \"", strerror(xerrno),
    " (EINVAL [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_write_with_error_test) {
  int res, xerrno;
  pr_fh_t *fh;
  size_t sz;
  const char *buf, *expected, *errstr;
  pr_error_t *err = NULL;

  fh = NULL;
  buf = NULL;
  sz = 0;

  res = pr_fsio_write_with_error(NULL, fh, buf, sz, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with write");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_write_with_error(p, fh, buf, sz, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with write");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  res = pr_fsio_write_with_error(NULL, fh, buf, sz, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with write");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_write_with_error(p, fh, buf, sz, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with write");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "write() failed with \"", strerror(xerrno),
    " (EINVAL [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_unlink_with_error_test) {
  int res, xerrno;
  const char *expected, *errstr, *path;
  pr_error_t *err = NULL;

  path = "/no/such/path";

  res = pr_fsio_unlink_with_error(NULL, path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_unlink_with_error(p, path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_unlink_with_error(NULL, path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_unlink_with_error(p, path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "unlink() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_rename_with_error_test) {
  int res, xerrno;
  const char *expected, *errstr, *old_path, *new_path;
  pr_error_t *err = NULL;

  old_path = "/no/such/path/to/old";
  new_path = "/no/such/path/to/new";

  res = pr_fsio_rename_with_error(NULL, old_path, new_path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with renaming '%s'", old_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_rename_with_error(p, old_path, new_path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with renaming '%s'", old_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_rename_with_error(NULL, old_path, new_path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with renaming '%s'", old_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_rename_with_error(p, old_path, new_path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with renaming '%s'", old_path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "rename() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_stat_with_error_test) {
  int res, xerrno;
  const char *expected, *errstr, *path;
  pr_error_t *err = NULL;
  struct stat st;

  path = "/no/such/path";

  res = pr_fsio_stat_with_error(NULL, path, &st, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with stat of '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_stat_with_error(p, path, &st, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with stat of '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_stat_with_error(NULL, path, &st, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with stat of '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_stat_with_error(p, path, &st, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with stat of '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "stat() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_mkdir_with_error_test) {
  int res, xerrno;
  mode_t mode;
  const char *expected, *errstr, *path;
  pr_error_t *err = NULL;

  path = "/no/such/path";
  mode = 0157;

  res = pr_fsio_mkdir_with_error(NULL, path, mode, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_mkdir_with_error(p, path, mode, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_mkdir_with_error(NULL, path, mode, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_mkdir_with_error(p, path, mode, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "mkdir() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

START_TEST (fsio_rmdir_with_error_test) {
  int res, xerrno;
  const char *expected, *errstr, *path;
  pr_error_t *err = NULL;

  path = "/no/such/path";

  res = pr_fsio_rmdir_with_error(NULL, path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_rmdir_with_error(p, path, NULL);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  res = pr_fsio_rmdir_with_error(NULL, path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err == NULL, "Unexpectedly got error back");

  res = pr_fsio_rmdir_with_error(p, path, &err);
  fail_unless(res < 0, "Unexpectedly succeeded with path '%s'", path);
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
  fail_unless(err != NULL, "Failed to get error back");

  mark_point();
  xerrno = errno;
  errstr = pr_error_strerror(err, PR_ERROR_FORMAT_USE_MINIMAL);
  expected = pstrcat(p, "rmdir() failed with \"", strerror(xerrno),
    " (ENOENT [", get_errnum(p, xerrno), "])\"", NULL);
  fail_unless(strcmp(expected, errstr) == 0, "Expected '%s', got '%s'",
    expected, errstr);

  pr_error_destroy(err);
}
END_TEST

Suite *tests_get_fsio_err_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("fsio-err");
  testcase = tcase_create("base");
  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, fsio_open_with_error_test);
  tcase_add_test(testcase, fsio_close_with_error_test);
  tcase_add_test(testcase, fsio_read_with_error_test);
  tcase_add_test(testcase, fsio_write_with_error_test);
  tcase_add_test(testcase, fsio_unlink_with_error_test);
  tcase_add_test(testcase, fsio_rename_with_error_test);
  tcase_add_test(testcase, fsio_stat_with_error_test);
  tcase_add_test(testcase, fsio_mkdir_with_error_test);
  tcase_add_test(testcase, fsio_rmdir_with_error_test);

  suite_add_tcase(suite, testcase);

  return suite;
}
