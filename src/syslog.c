#include <limits.h>
#define SYSLOG_NAMES 1 /* export facilitynames and prioritynames */
#include <valiant/syslog.h>

#define SIGN_BIT \
  (1<<(sizeof(int) * CHAR_BIT - 1))
#define LOG_TO_SYSLOG \
  (SIGN_BIT>>1)
#define LOG_TO_CONSOLE \
  (SIGN_BIT>>2)
#define LOG_PRIORITY \
  (LOG_EMERG | \
   LOG_ALERT | \
   LOG_CRIT | \
   LOG_ERR | \
   LOG_WARNING | \
   LOG_NOTICE | \
   LOG_INFO | \
   LOG_DEBUG)


static int syslog_mask = LOG_FLAG_CONSOLE;

static void
syslog_vprintf (int pri, const char *fmt, va_list ap)
  __attribute__((nonnull(2)));


#ifndef NDEBUG
static int
syslog_is_facility (int fac)
{
  CODE *slc;

  for (slc = facilitynames; slc->c_val != fac && slc->c_val >= 0; slc++) {
    /* do nothing */
  }

  return slc->c_val == fac;
}

static int
syslog_is_priority (int pri)
{
  CODE *slc;

  for (slc = prioritynames; slc->c_val != pri && slc->c_val >= 0; slc++) {
    /* do nothing */
  }

  return slc->c_val == pri;
}
#endif

int
syslog_xlat_facility (const char *name)
{
  CODE *sslc;
  int fac = -1;

  if (name != NULL) {
    for (slc = facilitynames; fac == -1 && slc->c_name; slc++) {
      if (strcmp (name, slc->c_name) == 0)
        fac = slc->c_val;
    }
  }

  return fac;
}

int
syslog_xlat_priority (const char *name)
{
  CODE *slc;
  int pri = -1;

  if (name != NULL) {
    for (slc = prioritynames; pri == -1 && slc->c_name; slc++) {
      if (strcmp (name, slc->c_name) == 0) {
        pri = slc->c_val;
      }
    }
  }

  return pri;
}

void
syslog_open (const char *ident, int fac)
{
  int mask;

  assert (fac == 0 || syslog_is_facility (fac));

  if (fac == 0) {
    fac = LOG_MAIL;
  }

  /* connection opened by future syslog call */
  openlog (ident, LOG_PID, fac);

  mask = syslog_mask | LOG_TO_SYSLOG;
  syslog_mask = mask;
}

void
syslog_close (void)
{
  int mask, orig_mask;

  orig_mask = syslog_mask;
  mask = orig_mask & ~LOG_TO_SYSLOG;
  syslog_mask = mask;

  if (orig_mask & LOG_TO_SYSLOG) {
    /* not responsible for thread synchronization */
    closelog ();
  }
}

int
syslog_set_level (int pri)
{
  int mask, orig_mask;

  assert (pri == 0 || syslog_is_priority (pri));

  orig_mask = syslog_mask;
  if (pri != 0) {
    mask = (orig_mask & ~LOG_PRIORITY) | pri;
    syslog_mask = mask;
  }

  return orig_mask & LOG_PRIORITY;
}

static void
syslog_set_console (int cons)
{
  int mask = syslog_mask;

  if (cons != 0) {
    mask |= LOG_TO_CONSOLE;
  } else {
    mask &= ~LOG_TO_CONSOLE;
  }

  syslog_mask = mask;
}

static void
syslog_vprintf (int pri, const char *fmt, va_list ap)
{
  int mask;
  FILE *file;

  assert (syslog_is_priority (pri));
  assert (fmt != NULL);

  mask = syslog_mask;

  if (mask & LOG_TO_SYSLOG) {
    vsyslog (pri, fmt, ap);
  }

  if (mask & LOG_TO_CONSOLE) {
    if (pri < LOG_INFO) {
      file = stderr;
    } else {
      file = stdout;
    }

    vfprintf (file, fmt, ap);
    fprintf (file, "\n");
  }
}

void
syslog_printf (int pri, const char *fmt, ...)
{
  assert (syslog_is_priority (pri));
  assert (fmt != NULL);

  va_list ap;
  va_start (ap, fmt);
  syslog_vprintf (pri, fmt, ap);
  va_end (ap);
}

void
panic (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  syslog_vprintf (LOG_EMERG, fmt, ap);
  va_end (ap);

  _exit (EXIT_FAILURE);
}

void
fatal (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  syslog_vprintf (LOG_CRIT, fmt, ap);
  va_end (ap);

  exit (EXIT_FAILURE);
  _exit (EXIT_FAILURE); /* in case exit fails */
}

