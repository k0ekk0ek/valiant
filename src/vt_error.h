#ifndef VT_ERROR_H_INCLUDED
#define VT_ERROR_H_INCLUDED

/* system includes */
#include <stdarg.h>
#include <errno.h>

#define VT_ERROR_BUFLEN (1024)

/* #define VT_SUCCESS (0) // obsolete */
/* #define VT_ERR_NOMEM (1) // obsolete, superseded by ENOMEM */
/* #define VT_ERR_NOBUFS (2) // obsolete, superseded by ENOBUFS */
/* #define VT_ERR_INVAL (3) // obsolete, superseded by EINVAL */
/* #define VT_ERR_QFULL (4) // obsolete */
/* #define VT_ERR_MAP (5) // obsolete */
/* #define VT_ERR_ALREADY (6) // obsolete, superseded by EALREADY */
/* #define VT_ERR_BADCFG (7) // obsolete */
/* #define VT_ERR_BADMBR (8) // obsolete */
/* #define VT_ERR_CONNFAILED (9) // obsolete */
/* #define VT_ERR_NORETRY (10) // obsolete */
/* #define VT_ERR_RETRY (11) // obsolete */
/* #define VT_ERR_AGAIN (12) // obsolete, superseded by EAGAIN */
/* #define VT_ERR_BADREQUEST (13) // obsolete */

#define VT_ERR_BAD_REQUEST (-1) /* Not a valid request */
#define VT_ERR_BAD_MEMBER (-2) /* Not a valid request member */

void vt_set_errno (int *, int);
char *vt_strerror (int);
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
