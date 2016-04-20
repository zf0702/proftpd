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

#ifndef PR_ERROR_H
#define PR_ERROR_H

#include "conf.h"
#include "pool.h"

typedef struct err_rec pr_error_t;

pr_error_t *pr_error_create(pool *p, int xerrno);
void pr_error_destroy(pr_error_t *err);

int pr_error_set_goal(pr_error_t *err, const char *goal);
int pr_error_set_location(pr_error_t *err, module *m, const char *file,
  int lineno);

unsigned int pr_error_use_details(unsigned int use_details);
#define PR_ERROR_DETAILS_USE_USER_NAME		0x00001
#define PR_ERROR_DETAILS_USE_USER_ID		0x00002
#define PR_ERROR_DETAILS_USE_GROUP_NAME		0x00004
#define PR_ERROR_DETAILS_USE_GROUP_ID		0x00008
#define PR_ERROR_DETAILS_USE_PROTOCOL		0x00010
#define PR_ERROR_DETAILS_USE_MODULE		0x00020
#define PR_ERROR_DETAILS_USE_FILE		0x00040
#define PR_ERROR_DETAILS_USE_LINENO		0x00080

#define PR_ERROR_DETAILS_DEFAULT \
  (PR_ERROR_DETAILS_USE_USER_NAME|PR_ERROR_DETAILS_USE_USER_ID| \
   PR_ERROR_DETAILS_USE_GROUP_NAME|PR_ERROR_DETAILS_USE_GROUP_ID| \
   PR_ERROR_DETAILS_USE_PROTOCOL|PR_ERROR_DETAILS_USE_MODULE| \
   PR_ERROR_DETAILS_USE_FILE|PR_ERROR_DETAILS_USE_LINENO)

unsigned int pr_error_use_formats(unsigned int use_formats);
#define PR_ERROR_FORMAT_USE_DETAILED		0x001
#define PR_ERROR_FORMAT_USE_TERSE		0x002
#define PR_ERROR_FORMAT_USE_MINIMAL		0x004

#define PR_ERROR_FORMAT_DEFAULT \
  (PR_ERROR_FORMAT_USE_DETAILED|PR_ERROR_FORMAT_USE_MINIMAL)

/* Convert the error into a textual representation (determined by format)
 * for consumption/use in e.g. logging.
 */
const char *pr_error_strerror(pr_error_t *err, int format);

/* Explain individual operations' errors.  The list of explainable operations
 * is NOT meant to be a comprehensive list of all system/library calls used
 * by ProFTPD and its modules.  Instead, the list of operations is meant
 * mostly for those operations whose failure will be user/admin-visible, AND
 * whose explanations can be useful for the user/admin for correcting the
 * cause of the problem.
 */

int pr_error_explain_accept(pr_error_t *err, int fd,
  struct sockaddr *addr, socklen_t *addr_len);

int pr_error_explain_bind(pr_error_t *err, int fd,
  const struct sockaddr *addr, socklen_t addr_len);

int pr_error_explain_chdir(pr_error_t *err, const char *path, mode_t mode);

int pr_error_explain_chmod(pr_error_t *err, const char *path, mode_t mode);

int pr_error_explain_chown(pr_error_t *err, const char *path,
  uid_t uid, gid_t gid);

int pr_error_explain_chroot(pr_error_t *err, const char *path);

int pr_error_explain_close(pr_error_t *err, int fd);

int pr_error_explain_closedir(pr_error_t *err, void *dirh);

int pr_error_explain_connect(pr_error_t *err, int fd,
  const struct sockaddr *addr, socklen_t addr_len);

int pr_error_explain_fchmod(pr_error_t *err, int fd, mode_t mode);

int pr_error_explain_fchown(pr_error_t *err, int fd, uid_t uid, gid_t gid);

int pr_error_explain_fclose(pr_error_t *err, FILE *fh);

int pr_error_explain_fcntl(pr_error_t *err, int fd, int oper, long arg);

int pr_error_explain_fdopen(pr_error_t *err, int fd, const char *mode);

int pr_error_explain_flock(pr_error_t *err, int fd, int oper);

int pr_error_explain_fopen(pr_error_t *err, const char *path, const char *mode);

int pr_error_explain_fork(pr_error_t *err);

int pr_error_explain_fstat(pr_error_t *err, int fd, struct stat *st);

int pr_error_explain_fstatfs(pr_error_t *err, int fd, void *stfs);

int pr_error_explain_fstatvfs(pr_error_t *err, int fd, void *stfs);

int pr_error_explain_fsync(pr_error_t *err, int fd);

int pr_error_explain_ftruncate(pr_error_t *err, int fd, off_t len);

int pr_error_explain_futimes(pr_error_t *err, int fd,
  const struct timeval *tvs);

int pr_error_explain_getaddrinfo(pr_error_t *err, const char *name,
  const char *service, const struct addrinfo *hints, struct addrinfo **res);

int pr_error_explain_gethostbyname(pr_error_t *err, const char *name);

int pr_error_explain_gethostbyname2(pr_error_t *err, const char *name,
  int family);

int pr_error_explain_gethostname(pr_error_t *err, char *buf, size_t sz);

int pr_error_explain_getnameinfo(pr_error_t *err, const struct sockaddr *addr,
  socklen_t addr_len, char *host, size_t host_len, char *service,
  size_t service_len, int flags);

int pr_error_explain_getpeername(pr_error_t *err, int fd, struct sockaddr *addr,
  socklen_t addr_len);

int pr_error_explain_getrlimit(pr_error_t *err, int resource,
  struct rlimit *rlim);

int pr_error_explain_getsockname(pr_error_t *err, struct sockaddr *addr,
  socklen_t *addr_len);

int pr_error_explain_getsockopt(pr_error_t *err, int fd, int level, int option,
  void *val, socklen_t *valsz);

int pr_error_explain_gettimeofday(pr_error_t *err, struct timeval *tv,
  void *tz);

int pr_error_explain_gmtime(pr_error_t *err, const time_t *then);

int pr_error_explain_lchmod(pr_error_t *err, const char *path, mode_t mode);

int pr_error_explain_lchown(pr_error_t *err, const char *path,
  uid_t uid, gid_t gid);

int pr_error_explain_link(pr_error_t *err, const char *target_path,
  const char *link_path);

int pr_error_explain_listen(pr_error_t *err, int fd, int backlog);

int pr_error_explain_localtime(pr_error_t *err, const time_t *then);

int pr_error_explain_lseek(pr_error_t *err, int fd, off_t offset, int whence);

int pr_error_explain_lstat(pr_error_t *err, const char *path, struct stat *st);

int pr_error_explain_lutimes(pr_error_t *err, const char *path,
  struct timeval *tvs);

int pr_error_explain_mkdir(pr_error_t *err, const char *path, mode_t mode);

int pr_error_explain_mkdtemp(pr_error_t *err, char *tmpl);

int pr_error_explain_mkstemp(pr_error_t *err, char *tmpl);

int pr_error_explain_open(pr_error_t *err, const char *path, int flags,
  mode_t mode);

int pr_error_explain_opendir(pr_error_t *err, const char *path);

int pr_eror_explain_read(pr_error_t *err, int fd, void *buf, size_t sz);

int pr_error_explain_readdir(pr_error_t *err, void *dirh);

int pr_error_explain_readlink(pr_error_t *err, const char *path, char *buf,
  size_t sz);

int pr_error_explain_readv(pr_error_t *err, int fd, void *buf, size_t sz);

int pr_error_explain_rename(pr_error_t *err, const char *old_path,
  const char *new_path);

int pr_error_explain_rmdir(pr_error_t *err, const char *path);

int pr_error_explain_setegid(pr_error_t *err, gid_t gid);

int pr_error_explain_seteuid(pr_error_t *err, uid_t uid);

int pr_error_explain_setgid(pr_error_t *err, gid_t gid);

int pr_error_explain_setregid(pr_error_t *err, gid_t rgid, gid_t egid);

int pr_error_explain_setresgid(pr_error_t *err, gid_t rgid, gid_t egid,
  gid_t sgid);

int pr_error_explain_setresuid(pr_error_t *err, uid_t ruid, uid_t euid,
  uid_t suid);

int pr_error_explain_setreuid(pr_error_t *err, uid_t ruid, uid_t euid);

int pr_error_explain_setrlimit(pr_error_t *err, int resource,
  const struct rlimit *rlim);

int pr_error_explain_setsockopt(pr_error_t *err, int fd, int level, int option,
  const void *val, socklen_t valsz);

int pr_error_explain_setuid(pr_error_t *err, uid_t uid);

int pr_error_explain_socket(pr_error_t *err, int domain, int type, int proto);

int pr_error_explain_stat(pr_error_t *err, const char *path, struct stat *st);

int pr_error_explain_statfs(pr_error_t *err, const char *path, void *stfs);

int pr_error_explain_statvfs(pr_error_t *err, const char *path, void *stfs);

int pr_error_explain_symlink(pr_error_t *err, const char *target_path,
  const char *link_path);

int pr_error_explain_time(pr_error_t *err, time_t *now);

int pr_error_explain_truncate(pr_error_t *err, const char *path, off_t len);

int pr_error_explain_unlink(pr_error_t *err, const char *path);

int pr_error_explain_utimes(pr_error_t *err, const char *path,
  const struct timeval *tvs);

int pr_error_explain_write(pr_error_t *err, int fd, const void *buf, size_t sz);

int pr_error_explain_writev(pr_error_t *err, int fd,
  const struct iovec *iov, int iov_len);

#endif /* PR_ERROR_H */
