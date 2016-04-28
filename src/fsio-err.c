/*
 * ProFTPD - FTP server daemon
 * Copyright (c) 2016 The ProFTPD Project
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
 * As a special exemption, The ProFTPD Project and other respective copyright
 * holders give permission to link this program with OpenSSL, and distribute
 * the resulting executable, without including the source code for OpenSSL in
 * the source distribution.
 */

/* ProFTPD FSIO API with error support */

#include "fsio-err.h"

int pr_fsio_mkdir_with_error(pool *p, const char *path, mode_t mode,
    pr_error_t **err) {
  int res;

  res = pr_fsio_mkdir(path, mode);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_mkdir(*err, path, mode);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_rmdir_with_error(pool *p, const char *path, pr_error_t **err) {
  int res;

  res = pr_fsio_rmdir(path);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_rmdir(*err, path);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_rename_with_error(pool *p, const char *rnfr, const char *rnto,
    pr_error_t **err) {
  int res;

  res = pr_fsio_rename(rnfr, rnto);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_rename(*err, rnfr, rnto);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_stat_with_error(pool *p, const char *path, struct stat *st,
    pr_error_t **err) {
  int res;

  res = pr_fsio_stat(path, st);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_stat(*err, path, st);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_unlink_with_error(pool *p, const char *path, pr_error_t **err) {
  int res;

  res = pr_fsio_unlink(path);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_unlink(*err, path);
    }

    errno = xerrno;
  }

  return res;
}

pr_fh_t *pr_fsio_open_with_error(pool *p, const char *name, int flags,
    pr_error_t **err) {
  pr_fh_t *fh;

  fh = pr_fsio_open(name, flags);
  if (fh == NULL) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_open(*err, name, flags, PR_OPEN_MODE);
    }

    errno = xerrno;
  }

  return fh;
}

int pr_fsio_close_with_error(pool *p, pr_fh_t *fh, pr_error_t **err) {
  int res;

  res = pr_fsio_close(fh);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      int fd = -1;

      *err = pr_error_create(p, xerrno);

      if (fh != NULL) {
        fd = fh->fh_fd;
      }

      (void) pr_error_explain_close(*err, fd);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_read_with_error(pool *p, pr_fh_t *fh, char *buf, size_t sz,
    pr_error_t **err) {
  int res;

  res = pr_fsio_read(fh, buf, sz);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      int fd = -1;

      if (fh != NULL) {
        fd = fh->fh_fd;
      }

      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_read(*err, fd, buf, sz);
    }

    errno = xerrno;
  }

  return res;
}

int pr_fsio_write_with_error(pool *p, pr_fh_t *fh, const char *buf, size_t sz,
    pr_error_t **err) {
  int res;

  res = pr_fsio_write(fh, buf, sz);
  if (res < 0) {
    int xerrno = errno;

    if (p != NULL &&
        err != NULL) {
      int fd = -1;

      if (fh != NULL) {
        fd = fh->fh_fd;
      }

      *err = pr_error_create(p, xerrno);
      (void) pr_error_explain_write(*err, fd, buf, sz);
    }

    errno = xerrno;
  }

  return res;
}
