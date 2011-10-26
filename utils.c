/* system includes */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/* valiant includes */
#include "utils.h"

void *
malloc0 (size_t nbytes)
{
  void *ptr;

  if ((ptr = malloc (nbytes)))
    memset (ptr, 0, nbytes);

  return ptr;
}

void
vt_log (int prio, const char *fmt, va_list ap)
{
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
