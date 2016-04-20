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

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = make_sub_pool(NULL);
  }
}

static void tear_down(void) {
  if (p) {
    destroy_pool(p);
    p = NULL;
  }
}

START_TEST (error_create_test) {
}
END_TEST

START_TEST (error_destroy_test) {
}
END_TEST

START_TEST (error_set_goal_test) {
}
END_TEST

START_TEST (error_set_location_test) {
}
END_TEST

START_TEST (error_strerror_test) {
  /* XXX Make sure to have tests for the details, formats */
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
  tcase_add_test(testcase, error_strerror_test);

#if 0
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
  tcase_add_test(testcase, error_explain_gmtime_test);
  tcase_add_test(testcase, error_explain_lchmod_test);
  tcase_add_test(testcase, error_explain_lchown_test);
  tcase_add_test(testcase, error_explain_link_test);
  tcase_add_test(testcase, error_explain_listen_test);
  tcase_add_test(testcase, error_explain_localtime_test);
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
