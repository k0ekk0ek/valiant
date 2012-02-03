#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"

int
main (int argc, char *argv[])
{
#define BUFLEN (4096)
  char rdbuf[BUFLEN];
  char *buf, *nbuf;
  int fd;
  int orig_errno;
  size_t cnt, len, nlen;
  ssize_t rdcnt;
  vt_lexer_t lxr;

  if (argc < 2) {
    fprintf (stderr, "not enough args\n");
    return 1;
  }

  if ((fd = open (argv[1], O_RDONLY)) < 0) {
    perror ("open");
    return 1;
  }

  buf = NULL;
  cnt = 0;
  len = 0;
  orig_errno = errno;
  errno = 0;

  for (;;) {
again:
    rdcnt = read (fd, rdbuf, BUFLEN);

    if (rdcnt > 0) {
      if (rdcnt > (len - cnt)) {
        nlen = len + rdcnt + 1;
        if (! (nbuf = realloc (buf, nlen))) {
          perror ("realloc");
          return 1;
        }
        buf = nbuf;
        len = nlen;
      }
      memcpy (buf+cnt, rdbuf, rdcnt);
      cnt += rdcnt;
      buf[cnt] = '\0';
    }

    if (rdcnt < BUFLEN) {
      if (errno) {
        if (errno == EINTR) {
          errno = 0;
          goto again;
        }
        perror ("read");
        return 1;
      }
      break; /* end of file */
    }
  }

  (void)close (fd);

  lxr.str = buf;
  lxr.len = cnt;
  lxr.lines = 1;
  lxr.columns = 1;

  vt_lexer_def_t def;
  vt_lexer_def_rst (&def);
  //lxr.def = &def;
  def.skip_comment_single = 0;
  def.int_as_float = 1;
vt_error ("%s", def.int_as_float ? "true" : "false");

  for (; vt_lexer_get_next_token (&lxr, &def, NULL); ) {
    printf ("token: %u on line %u, column %u\n", lxr.token, lxr.line, lxr.column);
    if (lxr.token == VT_TOKEN_EOF || lxr.token == VT_TOKEN_ERROR)
      break;
    if (lxr.token == VT_TOKEN_STRING) {
      printf ("string: '%s'\n", lxr.value.data.str);
    } else if (lxr.token == VT_TOKEN_BOOL) {
      printf ("boolean: %s\n", lxr.value.data.bln ? "true" : "false");
    } else if (lxr.token == VT_TOKEN_COMMENT_SINGLE || lxr.token == VT_TOKEN_COMMENT_MULTI) {
      printf ("comment: '%s'\n", lxr.value.data.str);
    } else if (lxr.value.type == VT_VALUE_TYPE_FLOAT) {
      printf ("float: %f\n", lxr.value.data.dbl);
    } else if (lxr.value.type == VT_VALUE_TYPE_INT) {
      printf ("int: %d\n", lxr.value.data.lng);
    }
  }

  printf ("bye!\n");

  return 0;
}
