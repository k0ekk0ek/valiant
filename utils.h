#ifndef VT_UTILS_H_INCLUDED
#define VT_UTILS_H_INCLUDED

/* system includes */
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdlib.h>

int reverse_inet_addr(char *, char *, socklen_t);
void *malloc0 (size_t);
void vt_panic (const char *, ...);
void vt_fatal (const char *, ...);
void vt_error (const char *, ...);
void vt_warning (const char *, ...);
void vt_info (const char *, ...);
void vt_debug (const char *, ...);

#endif
