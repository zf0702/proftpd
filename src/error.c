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
  const char *err_desc;

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
  const char *err_explained;
};

static unsigned int error_details = PR_ERROR_DETAILS_DEFAULT;
static unsigned int error_formats = PR_ERROR_FORMAT_DEFAULT;

struct errno_info {
  int error_number;
  const char *name;
};

static struct errno_info errno_names[] = {
#ifdef E2BIG
  { E2BIG, "E2BIG" },
#endif /* E2BIG */
#ifdef EACCES
  { EACCES, "EACCES" },
#endif /* EACCES */
#ifdef EADDRINUSE
  { EADDRINUSE, "EADDRINUSE" },
#endif /* EADDRINUSE */
#ifdef EADDRNOTAVAIL
  { EADDRNOTAVAIL, "EADDRNOTAVAIL" },
#endif /* EADDRNOTAVAIL */
#ifdef EAFNOSUPPORT
  { EAFNOSUPPORT, "EAFNOSUPPORT" },
#endif /* EAFNOSUPPORT */
#ifdef EAGAIN
  { EAGAIN, "EAGAIN" },
#endif /* EAGAIN */
#ifdef EALREADY
  { EALREADY, "EALREADY" },
#endif /* EALREADY */
#ifdef EBADF
  { EBADF, "EBADF" },
#endif /* EBADF */
#ifdef EBADFD
  { EBADFD, "EBADFD" },
#endif /* EBADFD */
#ifdef EBUSY
  { EBUSY, "EBUSY" },
#endif /* EBUSY */
#ifdef ECANCELED
  { ECANCELED, "ECANCELED" },
#endif /* ECANCELED */
#ifdef ECOMM
  { ECOMM, "ECOMM" },
#endif /* ECOMM */
#ifdef ECONNABORTED
  { ECONNABORTED, "ECONNABORTED" },
#endif /* ECONNABORTED */
#ifdef ECONNREFUSED
  { ECONNREFUSED, "ECONNREFUSED" },
#endif /* ECONNREFUSED */
#ifdef ECONNRESET
  { ECONNRESET, "ECONNRESET" },
#endif /* ECONNRESET */
#ifdef EDEADLK
  { EDEADLK, "EDEADLK" },
#endif /* EDEADLK */
#ifdef EDEADLOCK
  { EDEADLOCK, "EDEADLOCK" },
#endif /* EDEADLOCK */
#ifdef EDQUOT
  { EDQUOT, "EDQUOT" },
#endif /* EDQUOT */
#ifdef EEXIST
  { EEXIST, "EEXIST" },
#endif /* EEXIST */
#ifdef EFAULT
  { EFAULT, "EFAULT" },
#endif /* EFAULT */
#ifdef EFBIG
  { EFBIG, "EFBIG" },
#endif /* EFBIG */
#ifdef EHOSTDOWN
  { EHOSTDOWN, "EHOSTDOWN" },
#endif /* EHOSTDOWN */
#ifdef EHOSTUNREACH
  { EHOSTUNREACH, "EHOSTUNREACH" },
#endif /* EHOSTUNREACH */
#ifdef EILSEQ
  { EILSEQ, "EILSEQ" },
#endif /* EILSEQ */
#ifdef EINPROGRESS
  { EINPROGRESS, "EINPROGRESS" },
#endif /* EINPROGRESS */
#ifdef EINTR
  { EINTR, "EINTR" },
#endif /* EINTR */
#ifdef EINVAL
  { EINVAL, "EINVAL" },
#endif /* EINVAL */
#ifdef EISCONN
  { EISCONN, "EISCONN" },
#endif /* EISCONN */
#ifdef EISDIR
  { EISDIR, "EISDIR" },
#endif /* EISDIR */
#ifdef EIO
  { EIO, "EIO" },
#endif /* EIO */
#ifdef ELOOP
  { ELOOP, "ELOOP" },
#endif /* ELOOP */
#ifdef EMFILE
  { EMFILE, "EMFILE" },
#endif /* EMFILE */
#ifdef EMLINK
  { EMLINK, "EMLINK" },
#endif /* EMLINK */
#ifdef EMSGSIZE
  { EMSGSIZE, "EMSGSIZE" },
#endif /* EMSGSIZE */
#ifdef ENAMETOOLONG
  { ENAMETOOLONG, "ENAMETOOLONG" },
#endif /* ENAMETOOLONG */
#ifdef ENFILE
  { ENFILE, "ENFILE" },
#endif /* ENFILE */
#ifdef ENETDOWN
  { ENETDOWN, "ENETDOWN" },
#endif /* ENETDOWN */
#ifdef ENETRESET
  { ENETRESET, "ENETRESET" },
#endif /* ENETRESET */
#ifdef ENETUNREACH
  { ENETUNREACH, "ENETUNREACH" },
#endif /* ENETUNREACH */
#ifdef ENOBUFS
  { ENOBUFS, "ENOBUFS" },
#endif /* ENOBUFS */
#ifdef ENODATA
  { ENODATA, "ENODATA" },
#endif /* ENODATA */
#ifdef ENOATTR
  { ENOATTR, "ENOATTR" },
#endif /* ENOATTR */
#ifdef ENOLCK
  { ENOLCK, "ENOLCK" },
#endif /* ENOLCK */
#ifdef ENOLINK
  { ENOLINK, "ENOLINK" },
#endif /* ENOLINK */
#ifdef ENOMEDIUM
  { ENOMEDIUM, "ENOMEDIUM" },
#endif /* ENOMEDIUM */
#ifdef ENOMEM
  { ENOMEM, "ENOMEM" },
#endif /* ENOMEM */
#ifdef ENONET
  { ENONET, "ENONET" },
#endif /* ENONET */
#ifdef ENOTCONN
  { ENOTCONN, "ENOTCONN" },
#endif /* ENOTSCONN */
#ifdef ENOTEMPTY
  { ENOTEMPTY, "ENOTEMPTY" },
#endif /* ENOTEMPTY */
#ifdef ENOSPC
  { ENOSPC, "ENOSPC" },
#endif /* ENOSPC */
#ifdef ENOSYS
  { ENOSYS, "ENOSYS" },
#endif /* ENOSYS */
#ifdef ENXIO
  { ENXIO, "ENXIO" },
#endif /* ENXIO */
#ifdef ENOENT
  { ENOENT, "ENOENT" },
#endif /* ENOENT */
#ifdef ENOTDIR
  { ENOTDIR, "ENOTDIR" },
#endif /* ENOTDIR */
#ifdef ENOTSOCK
  { ENOTSOCK, "ENOTSOCK" },
#endif /* ENOTSOCK */
#ifdef ENOTSUP
  { ENOTSUP, "ENOTSUP" },
#endif /* ENOTSUP */
#ifdef EOPNOTSUPP
  { EOPNOTSUPP, "EOPNOTSUPP" },
#endif /* EOPNOTSUPP */
#ifdef EPERM
  { EPERM, "EPERM" },
#endif /* EPERM */
#ifdef EPFNOSUPPORT
  { EPFNOSUPPORT, "EPFNOSUPPORT" },
#endif /* EPFNOSUPPORT */
#ifdef EPIPE
  { EPIPE, "EPIPE" },
#endif /* EPIPE */
#ifdef EPROTO
  { EPROTO, "EPROTO" },
#endif /* EPROTO */
#ifdef EPROTONOSUPPORT
  { EPROTONOSUPPORT, "EPROTONOSUPPORT" },
#endif /* EPROTONOSUPPORT */
#ifdef EPROTOOPT
  { EPROTOOPT, "EPROTOOPT" },
#endif /* EPROTOOPT */
#ifdef EPROTOTYPE
  { EPROTOTYPE, "EPROTOTYPE" },
#endif /* EPROTOTYPE */
#ifdef ERANGE
  { ERANGE, "ERANGE" },
#endif /* ERANGE */
#ifdef EROFS
  { EROFS, "EROFS" },
#endif /* EROFS */
#ifdef ESHUTDOWN
  { ESHUTDOWN, "ESHUTDOWN" },
#endif /* ESHUTDOWN */
#ifdef ESPIPE
  { ESPIPE, "ESPIPE" },
#endif /* ESPIPE */
#ifdef ERESTART
  { ERESTART, "ERESTART" },
#endif /* ERESTART */
#ifdef ESRCH
  { ESRCH, "ESRCH" },
#endif /* ESRCH */
#ifdef ESTALE
  { ESTALE, "ESTALE" },
#endif /* ESTALE */
#ifdef ETIMEDOUT
  { ETIMEDOUT, "ETIMEDOUT" },
#endif /* ETIMEDOUT */
#ifdef ETXTBSY
  { ETXTBSY, "ETXTBSY" },
#endif /* ETXTBSY */
#ifdef EWOULDBLOCK
  { EWOULDBLOCK, "EWOULDBLOCK" },
#endif /* EWOULDBLOCK */
#ifdef EXDEV
  { EXDEV, "EXDEV" },
#endif /* EXDEV */

  { -1, NULL }
};

pr_error_t *pr_error_create(pool *p, int xerrno) {
  pr_error_t *err;
  pool *err_pool;

  /* Known errno values are not negative.  Right? */

  if (p == NULL ||
      xerrno < 0) {
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
    unsigned int lineno) {

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  err->err_module = m;
  err->err_file = file;
  err->err_lineno = lineno;

  return 0;
}

int pr_error_set_operation(pr_error_t *err, const char *oper,
    const char *args) {

  if (err == NULL ||
      oper == NULL) {
    errno = EINVAL;
    return -1;
  }

  err->err_oper = pstrdup(err->err_pool, oper);
  if (args != NULL) {
    err->err_args = pstrdup(err->err_pool, args);
  }

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

static const char *get_uid(char *uid, size_t uidsz) {
  memset(uid, '\0', uidsz);
  snprintf(uid, uidsz-1, "%lu", (unsigned long) session.uid);
  return uid;
}

static const char *get_gid(char *gid, size_t gidsz) {
  memset(gid, '\0', gidsz);
  snprintf(gid, gidsz-1, "%lu", (unsigned long) session.gid);
  return gid;
}

/* Returns string of:
 *
 *  "user ${user} (UID %{uid})/group ${group} (GID ${gid}) via ${protocol}"
 */
static const char *get_who(pr_error_t *err) {
  const char *who = NULL;

  if (session.user == NULL ||
      session.group == NULL) {
    /* Not logged in yet. */
    return who;
  }

  if (error_details & PR_ERROR_DETAILS_USE_NAMES) {
    who = pstrcat(err->err_pool, "user ", session.user, NULL);

    if (error_details & PR_ERROR_DETAILS_USE_IDS) {
      char uid[32];

      who = pstrcat(err->err_pool, who,
        " (UID ", get_uid(uid, sizeof(uid)), ")", NULL);
    }

    who = pstrcat(err->err_pool, who, "/group ", session.group, NULL);

    if (error_details & PR_ERROR_DETAILS_USE_IDS) {
      char gid[32];

      who = pstrcat(err->err_pool, who,
        " (GID ", get_uid(gid, sizeof(gid)), ")", NULL);
    }

  } else if (error_details & PR_ERROR_DETAILS_USE_IDS) {
    char uid[32], gid[32];

    who = pstrcat(err->err_pool, "UID ", get_uid(uid, sizeof(uid)),
      "/GID ", get_gid(gid, sizeof(gid)), NULL);
  }

  if (error_details & PR_ERROR_DETAILS_USE_PROTOCOL) {
    const char *proto;

    proto = pr_session_get_protocol(0);

    if (who != NULL) {
      who = pstrcat(err->err_pool, who, " via ", proto, NULL);

    } else {
      who = pstrcat(err->err_pool, "via ", proto, NULL);
    }
  }

  return who;
}

static const char *get_why(pr_error_t *err) {
  const char *why = NULL;

  if (err->err_goal != NULL) {
    why = err->err_goal;
  }

  return why;
}

/* Returns string of:
 *
 *  "in ${module} [${file}:${lineno}]"
 */
static const char *get_where(pr_error_t *err) {
  const char *where = NULL;

  if (error_details & PR_ERROR_DETAILS_USE_MODULE) {
    if (err->err_module != NULL) {
      where = pstrcat(err->err_pool, "in mod_", err->err_module->name, NULL);

    } else {
      where = pstrcat(err->err_pool, "in core", NULL);
    }
  }

  if (error_details & PR_ERROR_DETAILS_USE_FILE) {
    if (err->err_file != NULL) {
      int used_brackets = FALSE;

      if (where != NULL) {
        where = pstrcat(err->err_pool, " [", err->err_file, NULL);
        used_brackets = TRUE;

      } else {
        where = pstrcat(err->err_pool, "in ", err->err_file, NULL);
      }

      if (err->err_lineno > 0) {
        char linenum[32];

        memset(linenum, '\0', sizeof(linenum));
        snprintf(linenum, sizeof(linenum)-1, "%u", err->err_lineno);

        where = pstrcat(err->err_pool, where, ":", linenum,
          used_brackets ? "]" : "", NULL);

      } else {
        if (used_brackets) {
          where = pstrcat(err->err_pool, where, "]", NULL);
        }
      }
    }
  }

  return where;
}

static const char *get_what(pr_error_t *err) {
  const char *what = NULL;

  if (err->err_oper != NULL) {
    if (err->err_args != NULL) {
      what = pstrcat(err->err_pool, err->err_oper, " using ", err->err_args,
        NULL);

    } else {
      what = err->err_oper;
    }
  }

  return what;
}

/* TODO: Should this be implemented as one large switch statement instead? */
static const char *get_errno_name(int xerrno) {
  register unsigned int i;
  const char *name = NULL;

  /* Special-case handling for zero value. */
  if (xerrno == 0) {
    return "EOK";
  }

  for (i = 0; errno_names[i].name; i++) {
    if (errno_names[i].error_number == xerrno) {
      name = errno_names[i].name;
      break;
    }
  }

  if (name == NULL) {
    name = "<unknown/unsupported error>";
  }

  return name;
}

static const char *get_errno_desc(int xerrno) {
  const char *desc = NULL;

  /* Special-case handling for zero value. */
  if (xerrno != 0) {
    desc = strerror(xerrno);

  } else {
    desc = "Success";
  }

  return desc;
}

/* Returns string of:
 *
 *  "${err_desc} (${err_name} [${err_errno}])"
 */
static const char *get_failure(pr_error_t *err) {
  const char *failure = NULL;
  char errnum[32];

  memset(errnum, '\0', sizeof(errnum));
  snprintf(errnum, sizeof(errnum)-1, "%d", err->err_errno);

  if (err->err_name == NULL) {
    err->err_name = get_errno_name(err->err_errno);
  }

  if (err->err_desc == NULL) {
    err->err_desc = get_errno_desc(err->err_errno);
  }

  failure = pstrcat(err->err_pool, err->err_desc, " (", err->err_name,
    " [", errnum, "])", NULL);

  return failure;
}

static const char *get_explained(pr_error_t *err) {
  const char *explained = NULL;

  if (err->err_explained != NULL) {
    explained = err->err_explained;
  }

  return explained;
}

static const char *get_minimal_text(pool *p, const char *what,
    const char *failure) {
  const char *err_text = NULL;

  if (what != NULL) {
    err_text = what;
  }

  if (failure != NULL) {
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " failed with \"", failure, "\"", NULL);

    } else {
      err_text = failure;
    }
  }

  return err_text;
}

static const char *get_terse_text(pool *p, const char *what,
    const char *failure, const char *explained) {
  const char *err_text = NULL;

/* XXX TODO */

  return err_text;
}

static const char *get_detailed_text(pool *p, const char *who, const char *why,
    const char *where, const char *what, const char *failure,
    const char *explained) {
  const char *err_text = NULL;

  if (who != NULL) {
    err_text = who;
  }

  if (why != NULL) {
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " wanted to ", why, NULL);

    } else {
      err_text = why;
    }
  }

  if (where != NULL) {
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " in ", where, NULL);

    } else {
      err_text = pstrcat(p, "in ", where, NULL);
    }
  }

  if (what != NULL) {
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " but ", what, NULL);

    } else {
      err_text = what;
    }
  }

  if (failure != NULL) {
    /* Not much point in including the failure string if there is no other
     * context provided.
     */
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " failed with \"", failure, "\"", NULL);
    }
  }

  if (explained != NULL) {
    /* Not much point in including the failure explanation if there is no
     * other context provided.
     */
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " because ", explained, NULL);
    }
  }

  return err_text;
}

const char *pr_error_strerror(pr_error_t *err, int use_format) {
  const char *err_text = NULL;

  if (err == NULL) {
    return strerror(errno);
  }

  if (use_format == 0) {
    use_format = PR_ERROR_FORMAT_USE_DETAILED;
  }

  switch (use_format) {
    case PR_ERROR_FORMAT_USE_DETAILED:
      if (!(error_formats & PR_ERROR_FORMAT_USE_DETAILED)) {
        use_format = PR_ERROR_FORMAT_USE_TERSE;

      } else {
        break;
      }

    case PR_ERROR_FORMAT_USE_TERSE:
      if (!(error_formats & PR_ERROR_FORMAT_USE_TERSE)) {
        use_format = PR_ERROR_FORMAT_USE_MINIMAL;

      } else {
        break;
      }

    case PR_ERROR_FORMAT_USE_MINIMAL:
      break;

    default:
      /* We want to make sure that pr_error_strerror() ALWAYS returns a
       * non-NULL string.  So the fallback behavior is to just use
       * normal strerror(3).
       */
      return strerror(err->err_errno);
  }

  switch (use_format) {
    case PR_ERROR_FORMAT_USE_DETAILED: {
      const char *who, *why, *where, *what, *failure, *explained;

      who = get_who(err);
      why = get_why(err);
      where = get_where(err);
      what = get_what(err);
      failure = get_failure(err);
      explained = get_explained(err);

      err_text = get_detailed_text(err->err_pool, who, why, where, what,
        failure, explained);
      break;
    }

    case PR_ERROR_FORMAT_USE_TERSE: {
      const char *what, *failure, *explained;

      what = get_what(err);
      failure = get_failure(err);
      explained = get_explained(err);

      err_text = get_terse_text(err->err_pool, what, failure, explained);
      break;
    }

    case PR_ERROR_FORMAT_USE_MINIMAL: {
      const char *what, *failure;

      what = get_what(err);
      failure = get_failure(err);

      err_text = get_minimal_text(err->err_pool, what, failure);
      break;
    }
  }

  return err_text;
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

/* IFF err->err_errno is not 0 (OK), THEN call out to our explainers. */

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
