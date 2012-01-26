/* system includes */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "vt_config.h"
#include "vt_lexer.h"

vt_config_t *
vt_config_parse_buffer (vt_config_def_t *def,
                        const char *str,
                        size_t len,
                        vt_error_t *err)
{
  char *value;
  vt_config_t *cfg;
  vt_lexer_t *lxr;
  vt_lexer_token_t tok;

  vt_lexer_opts_t lxropts;

  lxropts.class = "abcdefghijklmnopqrstuvwxyz";
  lxropts.delim = "/\"'";
  lxropts.sep = ",";

  if (! (lxr = vt_lexer_create (str, len, err))) {
    goto failure;
  }
vt_error ("start parsing!\n");
  // now we can start parsing this stuff!
  for (; (tok = vt_lexer_get_token (lxr, &lxropts, err)) != VT_TOKEN_EOF
   && tok != VT_TOKEN_ERROR;) {
    //
    // just try something for know?!?!
    //
    value = vt_lexer_gets (lxr);
    vt_error ("token: %d, value: %s", tok, value);
  }

  if (tok == VT_TOKEN_ERROR) {
    vt_error ("ERROR!: %d", *err);
  }

failure:
  return NULL;
}


vt_config_t *
vt_config_parse_file (vt_config_def_t *def, const char *path, vt_error_t *err)
{
#define BUFLEN (4096)
  char rdbuf[BUFLEN];
  char *str;
  int fd;
  int orig_errno;
  size_t len;
  ssize_t nrd;
  vt_buf_t buf;
  vt_config_t *cfg;

  assert (def);
  assert (path);

  if ((fd = open (path, O_RDONLY)) < 0) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: open: %s", __func__, strerror (errno));
    goto failure;
  }
  if (vt_buf_init (&buf, BUFLEN) < 0) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto failure;
  }

  orig_errno = errno;
  errno = 0;
  for (;;) {
again:
    nrd = read (fd, rdbuf, BUFLEN);

    if (nrd > 0) {
      if (vt_buf_ncat (&buf, rdbuf, nrd) < 0) {
        vt_set_error (err, VT_ERR_NOMEM);
        vt_error ("%s: %s", __func__, strerror (errno));
        goto failure;
      }
    }
    if (nrd < BUFLEN) {
      if (errno) {
        if (errno == EINTR)
          goto again;
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s: read: %s", __func__, strerror (errno));
        goto failure;
      }
      break; /* end of file */
    }
  }

  (void)close (fd);
  errno = orig_errno;

  str = vt_buf_str (&buf);
  len = vt_buf_len (&buf);

  vt_error ("conf: %s\n", str);

  if (! (cfg = vt_config_parse_buffer (def, str, len, err)))
    goto failure;

  return cfg;
failure:
  // clean up
  return NULL;
#undef BUFLEN
}
