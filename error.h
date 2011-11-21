#ifndef VT_ERROR_H_INCLUDED
#define VT_ERROR_H_INCLUDED 1

/* system includes */
#include <errno.h>
#include <stdarg.h>

#define VT_SUCCESS (0)
#define VT_ERR_NOMEM (1)
#define VT_ERR_NOBUFS (2)
#define VT_ERR_CONNFAILED (3)
#define VT_ERR_INVAL (4)

typedef int vt_error_t;

void vt_set_error (vt_error_t *, vt_error_t);
int vt_syslog_factility (const char *); /* convert syslog facility into code */
int vt_syslog_priority (const char *); /* convert syslog priority into code */
void vt_open_syslog (const char *, int, int);
void vt_close_syslog (void);
void vt_log (int, const char *, va_list);
void vt_panic (const char *, ...);
void vt_fatal (const char *, ...);
void vt_error (const char *, ...);
void vt_warning (const char *, ...);
void vt_info (const char *, ...);
void vt_debug (const char *, ...);

#endif
