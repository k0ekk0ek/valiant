/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "buf.h"

int
vt_buf_init (vt_buf_t *buf, size_t len)
{
  assert (buf);

  memset (buf, 0, sizeof (vt_buf_t));
  if (len && ! (buf->buf = calloc (len, sizeof (char))))
    return -1;
  buf->len = len;
  buf->cnt = 0;
  return 0;
}

vt_buf_t *
vt_buf_ncpy (vt_buf_t *buf, const char *str, size_t len)
{
  char *nbuf;
  size_t nlen;

  assert (buf);
  assert (str);

  if (len) {
    if (! buf->buf || len >= buf->len) {
      // FIXME: handle integer overflow
      nlen = len + 1;
      if (! (nbuf = realloc (buf->buf, nlen * sizeof (char))))
        return NULL;
      buf->buf = nbuf;
      buf->len = nlen;
    }

    memmove (buf->buf, str, len);
    *(buf->buf+len) = '\0'; /* always null terminate */
    buf->cnt = len;
  }

  return buf;
}

void
vt_buf_deinit (vt_buf_t *buf)
{
  assert (buf);

  if (buf->buf)
    free (buf->buf);
  memset (buf, 0, sizeof (vt_buf_t));
}
