/*
 * ProFTPD - FTP server daemon
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

/* Error API */

#include "error.h"

struct err_rec {
  pool *err_pool;

  /* Actual errno value, or -1 if unknown */
  int err_errno;

  /* String of errno name, e.g. "EINVAL" */
  const char *err_name;

  /* strerror(3) value, or NULL if unknown. */
  const char *err_text;

  /* Module where the error occurred, if known. */
  module *err_module;

  /* File location of the error, e.g. __FILE__. */
  const char *err_file;

  /* Line number in file of the error, e.g. __LINE__. */
  unsigned int err_lineno;

  /* Components for use in a more detailed error message. */
  const char *err_goal;
  const char *err_oper;
  const char *err_args;
  const char *err_explanation;
};

static unsigned int error_details = PR_ERROR_DETAILS_DEFAULT;
static unsigned int error_formats = PR_ERROR_FORMAT_DEFAULT;

pr_error_t *pr_error_create(pool *p, int xerrno) {
  pr_error_t *err;
  pool *err_pool;

  if (p == NULL) {
    errno = EINVAL;
    return NULL;
  }

  err_pool = make_sub_pool(p);
  pr_pool_tag(err_pool, "error pool");

  err = pcalloc(err_pool, sizeof(pr_error_t));
  err->err_pool = err_pool;
  err->err_errno = xerrno;

  return err;
}

void pr_error_destroy(pr_error_t *err) {
  int xerrno;

  xerrno = errno;

  if (err != NULL) {
    destroy_pool(err->err_pool);
  }

  errno = xerrno;
  return;
}

int pr_error_set_goal(pr_error_t *err, const char *goal) {
  if (err == NULL ||
      goal == NULL) {
    errno = EINVAL;
    return -1;
  }

  err->err_goal = pstrdup(err->err_pool, goal);
  return 0;
}

int pr_error_set_location(pr_error_t *err, module *m, const char *file,
    int lineno) {

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  err->err_module = m;
  err->err_file = file;
  err->err_lineno = lineno;

  return 0;
}

unsigned int pr_error_use_details(unsigned int use_details) {
  unsigned int prev;

  prev = error_details;
  error_details = use_details;

  return prev;
}

unsigned int pr_error_use_formats(unsigned int use_formats) {
  unsigned int prev;

  prev = error_formats;
  error_formats = use_formats;

  return prev;
}

const char *pr_error_strerror(pr_error_t *err, int format) {

  if (err == NULL) {
    return strerror(errno);
  }

  /* XXX much work here; we defer provisioning many of the fields until now.
   *
   * Use pstrcat(), BUT use a ptr, rather than passing the start of the
   * string to pstrcat() each time.
   */

  return NULL;
}

int pr_error_explain_accept(pr_error_t *err, int fd, struct sockaddr *addr,
    socklen_t *addr_len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_bind(pr_error_t *err, int fd, const struct sockaddr *addr,
    socklen_t addr_len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_chdir(pr_error_t *err, const char *path, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_chmod(pr_error_t *err, const char *path, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_chown(pr_error_t *err, const char *path, uid_t uid,
    gid_t gid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_chroot(pr_error_t *err, const char *path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_close(pr_error_t *err, int fd) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_closedir(pr_error_t *err, void *dirh) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_connect(pr_error_t *err, int fd,
    const struct sockaddr *addr, socklen_t addr_len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fchmod(pr_error_t *err, int fd, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fchown(pr_error_t *err, int fd, uid_t uid, gid_t gid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fclose(pr_error_t *err, FILE *fh) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fcntl(pr_error_t *err, int fd, int oper, long arg) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fdopen(pr_error_t *err, int fd, const char *mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_flock(pr_error_t *err, int fd, int oper) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fopen(pr_error_t *err, const char *path,
    const char *mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fork(pr_error_t *err) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fstat(pr_error_t *err, int fd, struct stat *st) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fstatfs(pr_error_t *err, int fd, void *stfs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fstatvfs(pr_error_t *err, int fd, void *stfs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_fsync(pr_error_t *err, int fd) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_ftruncate(pr_error_t *err, int fd, off_t len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_futimes(pr_error_t *err, int fd,
    const struct timeval *tvs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getaddrinfo(pr_error_t *err, const char *name,
    const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_gethostbyname(pr_error_t *err, const char *name) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_gethostbyname2(pr_error_t *err, const char *name,
    int family) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_gethostname(pr_error_t *err, char *buf, size_t sz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getnameinfo(pr_error_t *err, const struct sockaddr *addr,
    socklen_t addr_len, char *host, size_t host_len, char *service,
    size_t service_len, int flags) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getpeername(pr_error_t *err, int fd, struct sockaddr *addr,
    socklen_t addr_len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getrlimit(pr_error_t *err, int resource,
    struct rlimit *rlim) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getsockname(pr_error_t *err, struct sockaddr *addr,
    socklen_t *addr_len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_getsockopt(pr_error_t *err, int fd, int level, int option,
    void *val, socklen_t *valsz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_gettimeofday(pr_error_t *err, struct timeval *tv,
    void *tz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_gmtime(pr_error_t *err, const time_t *then) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_lchmod(pr_error_t *err, const char *path, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_lchown(pr_error_t *err, const char *path, uid_t uid,
    gid_t gid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_link(pr_error_t *err, const char *target_path,
    const char *link_path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_listen(pr_error_t *err, int fd, int backlog) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_localtime(pr_error_t *err, const time_t *then) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_lseek(pr_error_t *err, int fd, off_t offset, int whence) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_lstat(pr_error_t *err, const char *path,
    struct stat *st) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_lutimes(pr_error_t *err, const char *path,
    struct timeval *tvs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_mkdir(pr_error_t *err, const char *path, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_mkdtemp(pr_error_t *err, char *tmpl) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_mkstemp(pr_error_t *err, char *tmpl) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_open(pr_error_t *err, const char *path, int flags,
    mode_t mode) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_opendir(pr_error_t *err, const char *path) {
  errno = ENOSYS;
  return -1;
}

int pr_eror_explain_read(pr_error_t *err, int fd, void *buf, size_t sz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_readdir(pr_error_t *err, void *dirh) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_readlink(pr_error_t *err, const char *path, char *buf,
    size_t sz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_readv(pr_error_t *err, int fd, void *buf, size_t sz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_rename(pr_error_t *err, const char *old_path,
    const char *new_path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_rmdir(pr_error_t *err, const char *path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setegid(pr_error_t *err, gid_t gid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_seteuid(pr_error_t *err, uid_t uid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setgid(pr_error_t *err, gid_t gid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setregid(pr_error_t *err, gid_t rgid, gid_t egid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setresgid(pr_error_t *err, gid_t rgid, gid_t egid,
    gid_t sgid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setresuid(pr_error_t *err, uid_t ruid, uid_t euid,
    uid_t suid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setreuid(pr_error_t *err, uid_t ruid, uid_t euid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setrlimit(pr_error_t *err, int resource,
    const struct rlimit *rlim) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setsockopt(pr_error_t *err, int fd, int level, int option,
    const void *val, socklen_t valsz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_setuid(pr_error_t *err, uid_t uid) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_socket(pr_error_t *err, int domain, int type, int proto) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_stat(pr_error_t *err, const char *path, struct stat *st) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_statfs(pr_error_t *err, const char *path, void *stfs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_statvfs(pr_error_t *err, const char *path, void *stfs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_symlink(pr_error_t *err, const char *target_path,
    const char *link_path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_truncate(pr_error_t *err, const char *path, off_t len) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_unlink(pr_error_t *err, const char *path) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_utimes(pr_error_t *err, const char *path,
    const struct timeval *tvs) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_write(pr_error_t *err, int fd, const void *buf,
    size_t sz) {
  errno = ENOSYS;
  return -1;
}

int pr_error_explain_writev(pr_error_t *err, int fd,
    const struct iovec *iov, int iov_len) {
  errno = ENOSYS;
  return -1;
}
