#ifndef _UTILS_H_INCLUDED_
#define _UTILS_H_INCLUDED_

/* system includes */

#include <stdarg.h>

#define EMPTY (0)
#define ERROR (-1)
#define DATA (1)

void bail (char *, ...);
void panic (char *, ...);
void error (char *, ...);
int readline(int fd, void *vptr, size_t maxlen);

#endif
