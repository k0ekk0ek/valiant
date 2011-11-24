#ifndef VT_ERROR_H_INCLUDED
#define VT_ERROR_H_INCLUDED 1

/* system includes */
#include <stdarg.h>

#define VT_SUCCESS (0)
#define VT_ERR_NOMEM (1)
#define VT_ERR_NOBUFS (2)
#define VT_ERR_INVAL (3)
#define VT_ERR_QFULL (4)
#define VT_ERR_MAP (5)
#define VT_ERR_ALREADY (6)
#define VT_ERR_BADCFG (7)
#define VT_ERR_BADMBR (8)
#define VT_ERR_CONNFAILED (9)
#define VT_ERR_NORETRY (10) /* permanent error */
#define VT_ERR_RETRY (11) /* temporary error */
#define VT_ERR_AGAIN (12)

typedef int vt_error_t;

void vt_set_error (vt_error_t *, vt_error_t);
int vt_syslog_factility (const char *); /* convert syslog facility into code */
int vt_syslog_priority (const char *); /* convert syslog priority into code */
void vt_syslog_open (const char *, int, int);
void vt_syslog_close (void);
void vt_log (int, const char *, va_list);
void vt_panic (const char *, ...);
void vt_fatal (const char *, ...);
void vt_error (const char *, ...);
void vt_warning (const char *, ...);
void vt_info (const char *, ...);
void vt_debug (const char *, ...);

#endif
