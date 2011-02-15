/* system includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* valiant includes */
#include "utils.h"


void
bail (char *fmt, ...)
{
  va_list vl;

  va_start (vl, fmt);
  vfprintf (stderr, fmt, vl);
  va_end (vl);

  exit (1);
}


void
panic (char *fmt, ...)
{
  va_list vl;

  va_start (vl, fmt);
  vfprintf (stderr, fmt, vl);
  va_end (vl);

  exit (1);
}

void
error (char *fmt, ...)
{
  va_list vl;

  va_start (vl, fmt);
  vfprintf (stderr, fmt, vl);
  va_end (vl);

  return;
}

/*
 * readline	- implementation by W. Richard Stevens, 
 * modified to not include line terminator (\n or \r\n)
 */
int
readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t n, rc;
	char c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
	      again:
		if ((rc = read(fd, &c, 1)) == 1) {
			if (c == '\n')
				break;	/* we don't want newline */
			if (!(c == '\r'))	/* we don't need these either */
				*ptr++ = c;
		} else if (rc == 0) {
			if (n == 1)
				return EMPTY;	/* EOF, no data read */
			else
				break;	/* EOF, some data read */
		} else {
			if (errno == EINTR)
				goto again;
			return ERROR;	/* error, errno set by read() */
		}
	}

	*ptr = 0;		/* null terminate like fgets() */
	return DATA;
}

