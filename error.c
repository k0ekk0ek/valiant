/* system includes */
#include <stdio.h>
#include <stdlib.h>
#define SYSLOG_NAMES 1
#include <syslog.h>

/* valiant includes */
#include "error.h"

static int _vt_syslog_facility = -1;
static int _vt_syslog_prio = -1;

void
vt_set_error (vt_error_t *dst, vt_error_t src)
{
  if (dst)
    *dst = src;
}

int
vt_syslog_facility (const char *name)
{
  CODE *slc;

  if (! name)
    return -1;
  for (slc = facilitynames; slc->c_name; slc++) {
    if (strcmp (name, slc->c_name) == 0)
      return slc->c_val;
  }
  return -1;
}

int
vt_syslog_priority (const char *name)
{
  CODE *slc;

  if (! name)
    return -1;
  for (slc = prioritynames; slc->c_name; slc++) {
    if (strcmp (name, slc->c_name) == 0)
      return slc->c_val;
  }
  return -1;
}

void
vt_syslog_open (const char *ident, int facility, int prio)
{
  if (_vt_syslog_facility < 0 && _vt_syslog_prio < 0) {
    openlog (ident, LOG_PID, facility);
    _vt_syslog_facility = facility;
    _vt_syslog_prio = prio;
  }
}

void
vt_syslog_close (void)
{
  if (_vt_syslog_facility >= 0 && _vt_syslog_prio >= 0) {
    closelog ();
    _vt_syslog_facility = -1;
    _vt_syslog_prio = -1;
  }
}

void
vt_log (int prio, const char *fmt, va_list ap)
{
  if (_vt_syslog_prio >= 0 && prio > _vt_syslog_prio)
    return;

  if (_vt_syslog_facility >= 0) {
    syslog (LOG_MAKEPRI (_vt_syslog_facility, prio), fmt, ap);
  } else if (prio < LOG_INFO) {
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
