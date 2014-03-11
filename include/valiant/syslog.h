#ifndef SYSLOG_H_INCLUDED
#define SYSLOG_H_INCLUDED 1

#include <stdarg.h>
#include <syslog.h>


extern int syslog_mask; /* do NOT modify direclty */

int syslog_xlat_facility (const char *)
  __attribute__ ((nonnull));

int syslog_xlat_priority (const char *)
  __attribute__ ((nonnull));

// FIXME: document no checking is done
void syslog_open (const char *, int)
  __attribute__((nonnull));

// FIXME: document no checking is done
void syslog_close (void);

int syslog_set_level (int);

void syslog_set_console (int);

void syslog_printf (int, const char *, ...)
  __attribute__((nonnull(2), format(printf,2,3)));

#define syslog_maybe_printf(pri, fmt, ...) \
  (syslog_mask & pri == pri ? syslog_printf (pri, fmt, __VA_ARGS__) : (void)0)

#define error(fmt, ...) syslog_maybe_printf (LOG_ERROR, fmt, __VA_ARGS__)
#define warning(fmt, ...) syslog_maybe_printf (LOG_WARNING, fmt, __VA_ARGS__)
#define notice(fmt, ...) syslog_maybe_printf (LOG_NOTICE, fmt, __VA_ARGS__)

void panic (const char *, ...)
  __attribute__((nonnull(1), format(printf,1,2)));
void fatal (const char *, ...)
  __attribute__((nonnull(1), format(printf,1,2)));

#ifndef NDEBUG
#define debug(fmt, ...) syslog_printf (LOG_DEBUG, fmt, __VA_ARGS__)
#else
/* no debug reporting in release builds */
#define debug(fmt, ...)
#endif

#endif /* SYSLOG_H_INCLUDED */
