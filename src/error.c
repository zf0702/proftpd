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

  /* Process identity at time of error. */
  const char *err_user;
  uid_t err_uid;
  gid_t err_gid;

  /* Components for use in a more detailed error message. */
  const char *err_goal;
  const char *err_oper;
  const char *err_args;
  const char *err_explained;
};

static unsigned int error_details = PR_ERROR_DETAILS_DEFAULT;
static unsigned int error_formats = PR_ERROR_FORMAT_DEFAULT;

struct err_explainer {
  struct err_explainer *next, *prev;

  module *m;
  const char *name;
  pr_error_explanations_t *explainers;
};

/* List of registered explainers. */
static struct err_explainer *error_explainers = NULL;

/* Currently selected explainers. */
static struct err_explainer *error_explainer = NULL;

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

static const char *trace_channel = "error";

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

  if (session.user != NULL) {
    err->err_user = pstrdup(err_pool, session.user);
  }

  /* NOTE: Should we get the real UID/GID here too? */
  err->err_uid = geteuid();
  err->err_gid = getegid();

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

int pr_error_get_identity(pr_error_t *err, uid_t *err_uid, gid_t *err_gid) {
  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (err_uid == NULL &&
      err_gid == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (err_uid != NULL) {
    *err_uid = err->err_uid;
  }

  if (err_gid != NULL) {
    *err_gid = err->err_gid;
  }

  return 0;
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

int pr_error_set_operation(pr_error_t *err, const char *oper) {
  if (err == NULL ||
      oper == NULL) {
    errno = EINVAL;
    return -1;
  }

  err->err_oper = pstrdup(err->err_pool, oper);
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

static const char *get_uid(pr_error_t *err, char *uid, size_t uidsz) {
  memset(uid, '\0', uidsz);
  snprintf(uid, uidsz-1, "%lu", (unsigned long) err->err_uid);
  return uid;
}

static const char *get_gid(pr_error_t *err, char *gid, size_t gidsz) {
  memset(gid, '\0', gidsz);
  snprintf(gid, gidsz-1, "%lu", (unsigned long) err->err_gid);
  return gid;
}

/* Returns string of:
 *
 *  "user ${user} (UID ${uid}, GID ${gid}) via ${protocol}"
 */
static const char *get_who(pr_error_t *err) {
  const char *who = NULL;

  if (error_details & PR_ERROR_DETAILS_USE_NAMES) {
    if (err->err_user != NULL) {
      who = pstrcat(err->err_pool, "user ", err->err_user, NULL);
    }

    if (error_details & PR_ERROR_DETAILS_USE_IDS) {
      char uid[32];

      if (err->err_user != NULL) {
        who = pstrcat(err->err_pool, who,
          " (UID ", get_uid(err, uid, sizeof(uid)), ",", NULL);

      } else {
        who = pstrcat(err->err_pool, "UID ", get_uid(err, uid, sizeof(uid)),
          ",", NULL);
      }
    }

    if (error_details & PR_ERROR_DETAILS_USE_IDS) {
      char gid[32];

      who = pstrcat(err->err_pool, who, " GID ",
        get_gid(err, gid, sizeof(gid)), NULL);
      if (err->err_user != NULL) {
        who = pstrcat(err->err_pool, who, ")", NULL);
      }
    }

  } else if (error_details & PR_ERROR_DETAILS_USE_IDS) {
    char uid[32], gid[32];

    who = pstrcat(err->err_pool, "UID ", get_uid(err, uid, sizeof(uid)),
      ", GID ", get_gid(err, gid, sizeof(gid)), NULL);
  }

  if (error_details & PR_ERROR_DETAILS_USE_PROTOCOL) {
    /* If we don't have a session.user, then we don't have a connection, and
     * thus we do not a protocol.
     */
    if (session.user != NULL) {
      const char *proto;

      proto = pr_session_get_protocol(0);

      if (who != NULL) {
        who = pstrcat(err->err_pool, who, " via ", proto, NULL);

      } else {
        who = pstrcat(err->err_pool, "via ", proto, NULL);
      }
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
 *  "${module} [${file}:${lineno}]"
 */
static const char *get_where(pr_error_t *err) {
  const char *where = NULL;

  if (error_details & PR_ERROR_DETAILS_USE_MODULE) {
    if (err->err_module != NULL) {
      where = pstrcat(err->err_pool, "mod_", err->err_module->name, NULL);

    } else {
      where = pstrdup(err->err_pool, "core");
    }
  }

  if (error_details & PR_ERROR_DETAILS_USE_FILE) {
    if (err->err_file != NULL) {
      int used_brackets = FALSE;

      if (where != NULL) {
        where = pstrcat(err->err_pool, where, " [", err->err_file, NULL);
        used_brackets = TRUE;

      } else {
        where = pstrcat(err->err_pool, err->err_file, NULL);
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

static const char *get_oper(pr_error_t *err) {
  const char *what = NULL;

  if (err->err_oper != NULL) {
    what = err->err_oper;
  }

  return what;
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

  if (what != NULL) {
    err_text = what;
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

static const char *get_detailed_text(pool *p, const char *where,
    const char *who, const char *why, const char *what, const char *failure,
    const char *explained) {
  const char *err_text = NULL;

  if (where != NULL) {
    err_text = pstrcat(p, "in ", where, NULL);
  }

  if (who != NULL &&
      (what != NULL || where != NULL)) {
    /* Not much point in including who, if there is no what or where to
     * go with them.
     */

    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, ", ", who, NULL);

    } else {
      err_text = who;
    }
  }

  if (why != NULL) {
    if (err_text != NULL) {
      err_text = pstrcat(p, err_text, " wanted to ", why, NULL);

    } else {
      err_text = why;
    }
  }

  if (what != NULL) {
    if (err_text != NULL) {
      if (why != NULL) {
        err_text = pstrcat(p, err_text, " but ", what, NULL);

      } else {
        err_text = pstrcat(p, err_text, " attempting to ", what, NULL);
      }

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

      err_text = get_detailed_text(err->err_pool, where, who, why, what,
        failure, explained);
      break;
    }

    case PR_ERROR_FORMAT_USE_TERSE: {
      const char *what, *failure, *explained;

      /* For terse messages, we only want the operation, if available, and NOT
       * the args.
       */
      what = get_oper(err);
      failure = get_failure(err);
      explained = get_explained(err);

      err_text = get_terse_text(err->err_pool, what, failure, explained);
      break;
    }

    case PR_ERROR_FORMAT_USE_MINIMAL: {
      const char *what, *failure;

      what = get_oper(err);
      failure = get_failure(err);

      err_text = get_minimal_text(err->err_pool, what, failure);
      break;
    }
  }

  if (err_text == NULL) {
    return strerror(err->err_errno);
  }

  return err_text;
}

pr_error_explanations_t *pr_error_register_explanations(pool *p, module *m,
    const char *name) {
  struct err_explainer *ee;
  pr_error_explanations_t *explainers;

  if (p == NULL ||
      name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  /* Check for duplicate registrations. */
  if (error_explainers != NULL) {
    for (ee = error_explainers; ee; ee = ee->next) {
      if ((m == NULL || m == ee->m) &&
          (strcmp(name, ee->name) == 0)) {
        errno = EEXIST;
        return NULL;
      }
    }
  }

  ee = pcalloc(p, sizeof(struct err_explainer));
  ee->m = m;
  ee->name = pstrdup(p, name);
  explainers = pcalloc(p, sizeof(pr_error_explanations_t));
  ee->explainers = explainers;

  ee->next = error_explainers;
  if (error_explainers != NULL) {
    error_explainers->prev = ee;

  } else {
    error_explainers = ee;
  }

  if (error_explainer == NULL) {
    /* If this is the first set of explainers registered, they become the
     * de facto selected set of explainers.
     */
    error_explainer = ee;
  }

  return explainers;
}

int pr_error_unregister_explanations(pool *p, module *m, const char *name) {
  struct err_explainer *ee;
  int res = -1;

  (void) p;

  /* We need either module or name (or both); both cannot be NULL. */
  if (m == NULL &&
      name == NULL) {
    errno = EINVAL;
    return -1;
  }

  for (ee = error_explainers; ee; ee = ee->next) {
    if ((m == NULL || m == ee->m) &&
        (name == NULL || strcmp(name, ee->name) == 0)) {

      if (ee->prev != NULL) {
        ee->prev->next = ee->next;

      } else {
        /* This explainer is the head of the explainers list, so we need
         * to update the head pointer as well.
         */
        error_explainers = ee->next;
      }

      if (ee->next != NULL) {
        ee->next->prev = ee->prev;
      }

      ee->prev = ee->next = NULL;

      /* If the unregistered explanations are currently the default/selected
       * ones, make sure to set that pointer to NULL, too.
       */
      if (ee == error_explainer) {
        error_explainer = NULL;
      }

      res = 0;
    }
  }

  if (res < 0) {
    errno = ENOENT;
  }

  return res;
}

int pr_error_use_explanations(pool *p, module *m, const char *name) {
  struct err_explainer *ee;

  (void) p;

  if (error_explainers == NULL) {
    errno = EPERM;
    return -1;
  }

  if (name == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (error_explainer != NULL) {
    if ((m == NULL || m == error_explainer->m) &&
        (strcmp(name, error_explainer->name) == 0)) {
      return 0;
    }
  }

  for (ee = error_explainers; ee; ee = ee->next) {
    if ((m == NULL || m == ee->m) &&
        (strcmp(name, ee->name) == 0)) {
      error_explainer = ee;
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

/* Even if err_errno is 0 (OK), we will still call out to the registered
 * explanation providers (explainers).  Why?
 *
 * An explanation provider, not the core API, is responsible for providing
 * a textual description of the operation's arguments, if nothing else.  Thus
 * even for an "OK" errno value, the caller might want the full textual
 * description of the operation and its arguments.
 */

static void trace_explained_error(module *m, const char *name,
    const char *oper, int xerrno) {

  if (m != NULL) {
    (void) pr_trace_msg(trace_channel, 9,
      "'%s' explanations (from mod_%s), failed to explain '%s': %s", name,
      m->name, oper, strerror(xerrno));

  } else {
    pr_trace_msg(trace_channel, 9,
      "'%s' explanations (from core), failed to explain '%s': %s", name,
      oper, strerror(xerrno));
  }
}

int pr_error_explain_accept(pr_error_t *err, int fd, struct sockaddr *addr,
    socklen_t *addr_len) {
  const char *oper = "accept()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_accept != NULL) {
      explained = (error_explainer->explainers->explain_accept)(err->err_pool,
        err->err_errno, fd, addr, addr_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_bind(pr_error_t *err, int fd, const struct sockaddr *addr,
    socklen_t addr_len) {
  const char *oper = "bind()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_bind != NULL) {
      explained = (error_explainer->explainers->explain_bind)(err->err_pool,
        err->err_errno, fd, addr, addr_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_chdir(pr_error_t *err, const char *path) {
  const char *oper = "chdir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_chdir != NULL) {
      explained = (error_explainer->explainers->explain_chdir)(err->err_pool,
        err->err_errno, path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_chmod(pr_error_t *err, const char *path, mode_t mode) {
  const char *oper = "chmod()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_chmod != NULL) {
      explained = (error_explainer->explainers->explain_chmod)(err->err_pool,
        err->err_errno, path, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_chown(pr_error_t *err, const char *path, uid_t uid,
    gid_t gid) {
  const char *oper = "chown()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_chown != NULL) {
      explained = (error_explainer->explainers->explain_chown)(err->err_pool,
        err->err_errno, path, uid, gid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_chroot(pr_error_t *err, const char *path) {
  const char *oper = "chroot()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_chroot != NULL) {
      explained = (error_explainer->explainers->explain_chroot)(err->err_pool,
        err->err_errno, path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_close(pr_error_t *err, int fd) {
  const char *oper = "close()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_close != NULL) {
      explained = (error_explainer->explainers->explain_close)(err->err_pool,
        err->err_errno, fd, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_closedir(pr_error_t *err, void *dirh) {
  const char *oper = "closedir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_closedir != NULL) {
      explained = (error_explainer->explainers->explain_closedir)(err->err_pool,
        err->err_errno, dirh, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_connect(pr_error_t *err, int fd,
    const struct sockaddr *addr, socklen_t addr_len) {
  const char *oper = "connect()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_connect != NULL) {
      explained = (error_explainer->explainers->explain_connect)(err->err_pool,
        err->err_errno, fd, addr, addr_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fchmod(pr_error_t *err, int fd, mode_t mode) {
  const char *oper = "fchmod()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fchmod != NULL) {
      explained = (error_explainer->explainers->explain_fchmod)(err->err_pool,
        err->err_errno, fd, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fchown(pr_error_t *err, int fd, uid_t uid, gid_t gid) {
  const char *oper = "fchown()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fchown != NULL) {
      explained = (error_explainer->explainers->explain_fchown)(err->err_pool,
        err->err_errno, fd, uid, gid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fclose(pr_error_t *err, FILE *fh) {
  const char *oper = "fclose()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fclose != NULL) {
      explained = (error_explainer->explainers->explain_fclose)(err->err_pool,
        err->err_errno, fh, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fcntl(pr_error_t *err, int fd, int op, long arg) {
  const char *oper = "fcntl()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fcntl != NULL) {
      explained = (error_explainer->explainers->explain_fcntl)(err->err_pool,
        err->err_errno, fd, op, arg, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fdopen(pr_error_t *err, int fd, const char *mode) {
  const char *oper = "fdopen()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fdopen != NULL) {
      explained = (error_explainer->explainers->explain_fdopen)(err->err_pool,
        err->err_errno, fd, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_flock(pr_error_t *err, int fd, int op) {
  const char *oper = "flock()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_flock != NULL) {
      explained = (error_explainer->explainers->explain_flock)(err->err_pool,
        err->err_errno, fd, op, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fopen(pr_error_t *err, const char *path,
    const char *mode) {
  const char *oper = "fopen()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fopen != NULL) {
      explained = (error_explainer->explainers->explain_fopen)(err->err_pool,
        err->err_errno, path, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fork(pr_error_t *err) {
  const char *oper = "fork()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fork != NULL) {
      explained = (error_explainer->explainers->explain_fork)(err->err_pool,
        err->err_errno, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fstat(pr_error_t *err, int fd, struct stat *st) {
  const char *oper = "fstat()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fstat != NULL) {
      explained = (error_explainer->explainers->explain_fstat)(err->err_pool,
        err->err_errno, fd, st, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fstatfs(pr_error_t *err, int fd, void *stfs) {
  const char *oper = "fstatfs()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fstatfs != NULL) {
      explained = (error_explainer->explainers->explain_fstatfs)(err->err_pool,
        err->err_errno, fd, stfs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fstatvfs(pr_error_t *err, int fd, void *stfs) {
  const char *oper = "fstatvfs()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fstatvfs != NULL) {
      explained = (error_explainer->explainers->explain_fstatvfs)(err->err_pool,
        err->err_errno, fd, stfs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_fsync(pr_error_t *err, int fd) {
  const char *oper = "fsync()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_fsync != NULL) {
      explained = (error_explainer->explainers->explain_fsync)(err->err_pool,
        err->err_errno, fd, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_ftruncate(pr_error_t *err, int fd, off_t len) {
  const char *oper = "ftruncate()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_ftruncate != NULL) {
      explained = (error_explainer->explainers->explain_ftruncate)(
        err->err_pool, err->err_errno, fd, len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_futimes(pr_error_t *err, int fd,
    const struct timeval *tvs) {
  const char *oper = "futimes()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_futimes != NULL) {
      explained = (error_explainer->explainers->explain_futimes)(err->err_pool,
        err->err_errno, fd, tvs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getaddrinfo(pr_error_t *err, const char *name,
    const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  const char *oper = "getaddrinfo()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getaddrinfo != NULL) {
      explained = (error_explainer->explainers->explain_getaddrinfo)(
        err->err_pool, err->err_errno, name, service, hints, res,
        &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_gethostbyname(pr_error_t *err, const char *name) {
  const char *oper = "gethostbyname()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_gethostbyname != NULL) {
      explained = (error_explainer->explainers->explain_gethostbyname)(
        err->err_pool, err->err_errno, name, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_gethostbyname2(pr_error_t *err, const char *name,
    int family) {
  const char *oper = "gethostbyname2()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_gethostbyname2 != NULL) {
      explained = (error_explainer->explainers->explain_gethostbyname2)(
        err->err_pool, err->err_errno, name, family, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_gethostname(pr_error_t *err, char *buf, size_t sz) {
  const char *oper = "gethostname()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_gethostname != NULL) {
      explained = (error_explainer->explainers->explain_gethostname)(
        err->err_pool, err->err_errno, buf, sz, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getnameinfo(pr_error_t *err, const struct sockaddr *addr,
    socklen_t addr_len, char *host, size_t host_len, char *service,
    size_t service_len, int flags) {
  const char *oper = "getnameinfo()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getnameinfo != NULL) {
      explained = (error_explainer->explainers->explain_getnameinfo)(
        err->err_pool, err->err_errno, addr, addr_len, host, host_len, service,
        service_len, flags, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getpeername(pr_error_t *err, int fd, struct sockaddr *addr,
    socklen_t *addr_len) {
  const char *oper = "getpeername()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getpeername != NULL) {
      explained = (error_explainer->explainers->explain_getpeername)(
        err->err_pool, err->err_errno, fd, addr, addr_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getrlimit(pr_error_t *err, int resource,
    struct rlimit *rlim) {
  const char *oper = "getrlimit()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getrlimit != NULL) {
      explained = (error_explainer->explainers->explain_getrlimit)(
        err->err_pool, err->err_errno, resource, rlim, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getsockname(pr_error_t *err, int fd, struct sockaddr *addr,
    socklen_t *addr_len) {
  const char *oper = "getsockname()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getsockname != NULL) {
      explained = (error_explainer->explainers->explain_getsockname)(
        err->err_pool, err->err_errno, fd, addr, addr_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_getsockopt(pr_error_t *err, int fd, int level, int option,
    void *val, socklen_t *valsz) {
  const char *oper = "getsockopt()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_getsockopt != NULL) {
      explained = (error_explainer->explainers->explain_getsockopt)(
        err->err_pool, err->err_errno, fd, level, option, val, valsz,
        &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_lchmod(pr_error_t *err, const char *path, mode_t mode) {
  const char *oper = "lchmod()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_lchmod != NULL) {
      explained = (error_explainer->explainers->explain_lchmod)(err->err_pool,
        err->err_errno, path, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_lchown(pr_error_t *err, const char *path, uid_t uid,
    gid_t gid) {
  const char *oper = "lchown()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_lchown != NULL) {
      explained = (error_explainer->explainers->explain_lchown)(err->err_pool,
        err->err_errno, path, uid, gid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_link(pr_error_t *err, const char *target_path,
    const char *link_path) {
  const char *oper = "link()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_link != NULL) {
      explained = (error_explainer->explainers->explain_link)(err->err_pool,
        err->err_errno, target_path, link_path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_listen(pr_error_t *err, int fd, int backlog) {
  const char *oper = "listen()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_listen != NULL) {
      explained = (error_explainer->explainers->explain_listen)(err->err_pool,
        err->err_errno, fd, backlog, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_lseek(pr_error_t *err, int fd, off_t offset, int whence) {
  const char *oper = "lseek()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_lseek != NULL) {
      explained = (error_explainer->explainers->explain_lseek)(err->err_pool,
        err->err_errno, fd, offset, whence, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_lstat(pr_error_t *err, const char *path,
    struct stat *st) {
  const char *oper = "lstat()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_lstat != NULL) {
      explained = (error_explainer->explainers->explain_lstat)(err->err_pool,
        err->err_errno, path, st, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_lutimes(pr_error_t *err, const char *path,
    struct timeval *tvs) {
  const char *oper = "lutimes()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_lutimes != NULL) {
      explained = (error_explainer->explainers->explain_lutimes)(err->err_pool,
        err->err_errno, path, tvs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_mkdir(pr_error_t *err, const char *path, mode_t mode) {
  const char *oper = "mkdir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_mkdir != NULL) {
      explained = (error_explainer->explainers->explain_mkdir)(err->err_pool,
        err->err_errno, path, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_mkdtemp(pr_error_t *err, char *tmpl) {
  const char *oper = "mkdtemp()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_mkdtemp != NULL) {
      explained = (error_explainer->explainers->explain_mkdtemp)(err->err_pool,
        err->err_errno, tmpl, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_mkstemp(pr_error_t *err, char *tmpl) {
  const char *oper = "mkstemp()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_mkstemp != NULL) {
      explained = (error_explainer->explainers->explain_mkstemp)(err->err_pool,
        err->err_errno, tmpl, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_open(pr_error_t *err, const char *path, int flags,
    mode_t mode) {
  const char *oper = "open()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_open != NULL) {
      explained = (error_explainer->explainers->explain_open)(err->err_pool,
        err->err_errno, path, flags, mode, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_opendir(pr_error_t *err, const char *path) {
  const char *oper = "opendir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_opendir != NULL) {
      explained = (error_explainer->explainers->explain_opendir)(err->err_pool,
        err->err_errno, path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_read(pr_error_t *err, int fd, void *buf, size_t sz) {
  const char *oper = "read()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_read != NULL) {
      explained = (error_explainer->explainers->explain_read)(err->err_pool,
        err->err_errno, fd, buf, sz, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_readdir(pr_error_t *err, void *dirh) {
  const char *oper = "readdir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_readdir != NULL) {
      explained = (error_explainer->explainers->explain_readdir)(err->err_pool,
        err->err_errno, dirh, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_readlink(pr_error_t *err, const char *path, char *buf,
    size_t sz) {
  const char *oper = "readlink()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_readlink != NULL) {
      explained = (error_explainer->explainers->explain_readlink)(err->err_pool,
        err->err_errno, path, buf, sz, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_readv(pr_error_t *err, int fd, const struct iovec *iov,
    int iov_len) {
  const char *oper = "readv()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_readv != NULL) {
      explained = (error_explainer->explainers->explain_readv)(err->err_pool,
        err->err_errno, fd, iov, iov_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_rename(pr_error_t *err, const char *old_path,
    const char *new_path) {
  const char *oper = "rename()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_rename != NULL) {
      explained = (error_explainer->explainers->explain_rename)(err->err_pool,
        err->err_errno, old_path, new_path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_rmdir(pr_error_t *err, const char *path) {
  const char *oper = "rmdir()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_rmdir != NULL) {
      explained = (error_explainer->explainers->explain_rmdir)(err->err_pool,
        err->err_errno, path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setegid(pr_error_t *err, gid_t gid) {
  const char *oper = "setegid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setegid != NULL) {
      explained = (error_explainer->explainers->explain_setegid)(err->err_pool,
        err->err_errno, gid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_seteuid(pr_error_t *err, uid_t uid) {
  const char *oper = "seteuid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_seteuid != NULL) {
      explained = (error_explainer->explainers->explain_seteuid)(err->err_pool,
        err->err_errno, uid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setgid(pr_error_t *err, gid_t gid) {
  const char *oper = "setgid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setgid != NULL) {
      explained = (error_explainer->explainers->explain_setgid)(err->err_pool,
        err->err_errno, gid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setregid(pr_error_t *err, gid_t rgid, gid_t egid) {
  const char *oper = "setregid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setregid != NULL) {
      explained = (error_explainer->explainers->explain_setregid)(err->err_pool,
        err->err_errno, rgid, egid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setresgid(pr_error_t *err, gid_t rgid, gid_t egid,
    gid_t sgid) {
  const char *oper = "setresgid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setresgid != NULL) {
      explained = (error_explainer->explainers->explain_setresgid)(
        err->err_pool, err->err_errno, rgid, egid, sgid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setresuid(pr_error_t *err, uid_t ruid, uid_t euid,
    uid_t suid) {
  const char *oper = "setresuid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setresuid != NULL) {
      explained = (error_explainer->explainers->explain_setresuid)(
        err->err_pool, err->err_errno, ruid, euid, suid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setreuid(pr_error_t *err, uid_t ruid, uid_t euid) {
  const char *oper = "setreuid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setreuid != NULL) {
      explained = (error_explainer->explainers->explain_setreuid)(err->err_pool,
        err->err_errno, ruid, euid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setrlimit(pr_error_t *err, int resource,
    const struct rlimit *rlim) {
  const char *oper = "setrlimit()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setrlimit != NULL) {
      explained = (error_explainer->explainers->explain_setrlimit)(
        err->err_pool, err->err_errno, resource, rlim, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setsockopt(pr_error_t *err, int fd, int level, int option,
    const void *val, socklen_t valsz) {
  const char *oper = "setsockopt()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setsockopt != NULL) {
      explained = (error_explainer->explainers->explain_setsockopt)(
        err->err_pool, err->err_errno, fd, level, option, val, valsz,
        &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_setuid(pr_error_t *err, uid_t uid) {
  const char *oper = "setuid()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_setuid != NULL) {
      explained = (error_explainer->explainers->explain_setuid)(err->err_pool,
        err->err_errno, uid, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_socket(pr_error_t *err, int domain, int type, int proto) {
  const char *oper = "socket()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_socket != NULL) {
      explained = (error_explainer->explainers->explain_socket)(err->err_pool,
        err->err_errno, domain, type, proto, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_stat(pr_error_t *err, const char *path, struct stat *st) {
  const char *oper = "stat()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_stat != NULL) {
      explained = (error_explainer->explainers->explain_stat)(err->err_pool,
        err->err_errno, path, st, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_statfs(pr_error_t *err, const char *path, void *stfs) {
  const char *oper = "statfs()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_statfs != NULL) {
      explained = (error_explainer->explainers->explain_statfs)(err->err_pool,
        err->err_errno, path, stfs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_statvfs(pr_error_t *err, const char *path, void *stfs) {
  const char *oper = "statvfs()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_statvfs != NULL) {
      explained = (error_explainer->explainers->explain_statvfs)(err->err_pool,
        err->err_errno, path, stfs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_symlink(pr_error_t *err, const char *target_path,
    const char *link_path) {
  const char *oper = "symlink()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_symlink != NULL) {
      explained = (error_explainer->explainers->explain_symlink)(err->err_pool,
        err->err_errno, target_path, link_path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_truncate(pr_error_t *err, const char *path, off_t len) {
  const char *oper = "truncate()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_truncate != NULL) {
      explained = (error_explainer->explainers->explain_truncate)(err->err_pool,
        err->err_errno, path, len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_unlink(pr_error_t *err, const char *path) {
  const char *oper = "unlink()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_unlink != NULL) {
      explained = (error_explainer->explainers->explain_unlink)(err->err_pool,
        err->err_errno, path, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_utimes(pr_error_t *err, const char *path,
    const struct timeval *tvs) {
  const char *oper = "utimes()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_utimes != NULL) {
      explained = (error_explainer->explainers->explain_utimes)(err->err_pool,
        err->err_errno, path, tvs, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_write(pr_error_t *err, int fd, const void *buf,
    size_t sz) {
  const char *oper = "write()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_write != NULL) {
      explained = (error_explainer->explainers->explain_write)(err->err_pool,
        err->err_errno, fd, buf, sz, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}

int pr_error_explain_writev(pr_error_t *err, int fd,
    const struct iovec *iov, int iov_len) {
  const char *oper = "writev()";

  if (err == NULL) {
    errno = EINVAL;
    return -1;
  }

  (void) pr_error_set_operation(err, oper);

  if (error_explainer != NULL) {
    const char *explained = NULL;
    int xerrno;

    if (error_explainer->explainers->explain_writev != NULL) {
      explained = (error_explainer->explainers->explain_writev)(err->err_pool,
        err->err_errno, fd, iov, iov_len, &(err->err_args));
      xerrno = errno;

    } else {
      xerrno = ENOSYS;
    }

    if (explained == NULL) {
      trace_explained_error(error_explainer->m, error_explainer->name,
        oper, xerrno);

      errno = xerrno;
      return -1;
    }

    err->err_explained = explained;
  }

  return 0;
}
