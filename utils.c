/* system includes */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SYSLOG_NAMES 1 /* needed for facilitynames and prioritynames */
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

/* valiant includes */
#include "settings.h"
#include "utils.h"

static int vt_facility = -1;
static int vt_priority = LOG_DEBUG;

void *
malloc0 (size_t nbytes)
{
  void *ptr;

  if ((ptr = malloc (nbytes)))
    memset (ptr, 0, nbytes);

  return ptr;
}

int
vt_log_factility (const char *name)
{
  CODE *slc;

  for (slc = facilitynames; slc->c_name; slc++) {
    if (strcmp (name, slc->c_name) == 0)
      return slc->c_val;
  }

  return -1;
}

int
vt_log_priority (const char *name)
{
  CODE *slc;

  for (slc = prioritynames; slc->c_name; slc++) {
    if (strcmp (name, slc->c_name) == 0)
      return slc->c_val;
  }

  return -1;
}

void
vt_log_open (int facility, int level)
{
  openlog ("valiant", LOG_PID, facility);
  vt_facility = facility;
  vt_priority = level;
}

void
vt_log (int prio, const char *fmt, va_list ap)
{
  //fprintf (stderr, "%d, %d\n", prio, vt_priority);
  if (prio > vt_priority)
    return;

  if (vt_facility >= 0) {
    syslog (LOG_MAKEPRI (vt_facility, prio), fmt, ap);
    return;
  }

  if (prio == LOG_EMERG ||
      prio == LOG_ALERT ||
      prio == LOG_CRIT  ||
      prio == LOG_WARNING)
  {
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
  } else {
    vfprintf (stdout, fmt, ap);
    fprintf (stdout, "\n");
  }
}

void
vt_panic (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vt_log (LOG_EMERG, fmt, ap);
  va_end (ap);

  exit (1);
}

void
vt_fatal (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vt_log (LOG_CRIT, fmt, ap);
  va_end (ap);

  exit (1);
}

void
vt_error (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vt_log (LOG_ERR, fmt, ap);
  va_end (ap);
}

void
vt_warning (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vt_log (LOG_WARNING, fmt, ap);
  va_end (ap);
}

void
vt_info (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vt_log (LOG_INFO, fmt, ap);
  va_end (ap);
}

void
vt_debug (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  vt_log (LOG_DEBUG, fmt, ap);
  va_end (ap);
}

/*
 * reverse_inet_addr - reverse ipaddress string for dnsbl query
 * e.g. 1.2.3.4 -> 4.3.2.1
 *
 * based on the reverse inet_addr function from gross
 */
int
reverse_inet_addr(char *src, char *dst, socklen_t size)
{
  unsigned int ipa, ripa;
  int i, ret;
  struct in_addr ina;
  const char *ptr;

  if ((ret = strlen (src)) >= INET_ADDRSTRLEN) {
    errno = EINVAL;
    return -1;
  }
  if ((ret = inet_pton (AF_INET, src, &ina)) != 1) {
    if (ret == 0)
      errno = EINVAL;
    return -1;
  }

  ipa = ina.s_addr;
  ripa = 0;

  for (i=0; i < 4; i++) {
    ripa <<= 8;
    ripa |= ipa & 0xff;
    ipa  >>= 8;
  }

  if (inet_ntop (AF_INET, &ripa, dst, size) == NULL) {
    return -1;
  }

  return 0;
}

#define BUFLEN (1024)
pid_t
pid_file_read (const char *path)
{
  int err, fd, ret;
  char buf[BUFLEN+1], *ptr;
  pid_t pid = 0;
  ssize_t nrd;

  for (; (fd = open (path, O_RDONLY)) < 0 && errno == EINTR;)
    ; /* open file, retry on EINTR */

  if (fd < 0)
    return (pid_t)-1;

  err = 0;
  for (;;) {
    nrd = read (fd, buf, BUFLEN);
    if (nrd < 0) {
      if (errno != EINTR) {
        err = errno;
        break;
      }
      /* ignore */
    } else if (nrd > 0) {
      buf[BUFLEN] = '\0'; /* null terminate to make this easier */
      for (ptr=buf; *ptr; ptr++) {
        if (pid) {
          if (isdigit (*ptr)) {
            pid = (pid * 10) + (*ptr - '0');
          } else if (isspace (*ptr)) {
            break; /* end of pid */
          } else {
            err = EINVAL;
            break;
          }
        } else {
          if (isdigit (*ptr)) {
            pid = *ptr - '0';
          } else if (! isspace (*ptr)) {
            err = EINVAL;
            break;
          }
        }
      }
    } else {
      break; /* end of file */
    }
  }

  for (; (ret = close (fd)) < 0 && errno == EINTR; )
    ;

  if (! pid && ! err)
    err = EINVAL;
  if (err) {
    errno = err;
    return (pid_t) -1;
  }

  return pid;
}

pid_t
pid_file_write (const char *path, pid_t pid)
{
  // FIXME: Use tempfile or something similar so the log file is removed when
  // the program terminates.
  char buf[BUFLEN];
  int err, fd;
  int i, n, nwn;
  mode_t cmask;

  if ((n = snprintf (buf, BUFLEN, "%d", pid)) < 0 || n > BUFLEN)
    return (pid_t) -1;

  for (; (fd = open (path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0 && errno == EINTR;)
    ;

  if (fd < 0)
    return (pid_t) -1;

  err = 0;
  for (i=0, errno=0; ! err && i < n; ) {
    nwn = write (fd, buf+i, n - i);
    if (nwn > 0)
      i += nwn;
    if ((n - i) && errno) {
      if (errno != EINTR)
        err = errno;
      errno = 0;
    }
  }

  if (err) {
    errno = err;
    return (pid_t) -1;
  }

  return pid;
}
#undef BUFLEN

/*
 * after example in advanced programming in the unix environment
 */
void
daemonize (vt_context_t *ctx)
{
  int fdin, fdout, fderr;
  pid_t pid;
  struct rlimit rl;
  int i;

  umask (0);

  if (getrlimit (RLIMIT_NOFILE, &rl) < 0)
    vt_fatal ("cannot get file limit");

  if ((pid = pid_file_read (ctx->pid_file)) == (pid_t) -1) {
    fprintf (stderr, "pid is %ld\n", pid);
    if (errno != ENOENT)
      vt_fatal ("cannot read %s: %s", ctx->pid_file, strerror (errno));
  } else if (kill (pid, 0) == 0) {
    fprintf (stderr, "running!\n");
    vt_fatal ("already running");
  } else if (unlink (ctx->pid_file) < 0) {
    vt_fatal ("cannot remove %s: %s", ctx->pid_file, strerror (errno));
  }
fprintf (stderr, "here!!!!\n");
  /* evertyhing should be set up to daemonize */

  if ((pid = fork ()) < 0) /* error */
    vt_fatal ("cannot fork process: %s", strerror (errno));
  if (pid > 0) /* parent */
    exit (EXIT_SUCCESS);
  if (setsid () == (pid_t) -1)
    vt_fatal ("cannot become session leader");
  if (pid_file_write (ctx->pid_file, (pid = getpid ())) == (pid_t) -1)
    vt_fatal ("cannot write %s: %s", ctx->pid_file, strerror (errno));

  /* Change working directory to / so we won't prevent file systems from being
     unmounted. */
  if (chdir ("/") < 0)
    vt_fatal ("cannot change to root directory");

  // close all open file descriptors
  if (rl.rlim_max == RLIM_INFINITY)
    rl.rlim_max = 1024;
  for (i=0; i < rl.rlim_max; i++)
    close (i);

  /* Attach STDIN, STDOUT, and STDERR to /dev/null. */
  fdin = open ("/dev/null", O_RDWR);
  fdout = dup (fdin);
  fderr = dup (fdin);

  vt_log_open (ctx->log_facility, ctx->log_level);
  if (fdin  != STDIN_FILENO  ||
      fdout != STDOUT_FILENO ||
      fderr != STDERR_FILENO)
    vt_fatal ("unexpected file descriptors %d %d %d", fdin, fdout, fderr);
}























































