/* system includes */
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* valiant includes */
#include "vt_lexer.h"

/* prototypes */
ssize_t vt_lexer_putc (vt_lexer_t *, char, vt_error_t *);
ssize_t vt_lexer_puts (vt_lexer_t *, char *, size_t, vt_error_t *);
ssize_t vt_lexer_clear (vt_lexer_t *, vt_error_t *);

vt_lexer_t *
vt_lexer_create (const char *str, size_t len, vt_error_t *err)
{
  vt_lexer_t *lxr;

  assert (str);

  if (! (lxr = calloc (1, sizeof (vt_lexer_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto failure;
  }
  if (vt_buf_init (&lxr->value, 3) < 0) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    goto failure;
  }

  lxr->str = str;
  lxr->len = len;
  lxr->state = VT_LEXER_STATE_NONE;

  return lxr;
failure:
  vt_lexer_destroy (lxr);
  return NULL;
}

void vt_lexer_destroy (vt_lexer_t *lxr)
{
  if (lxr) {
    vt_buf_deinit (&lxr->value);
    free (lxr);
  }
}

char *
vt_lexer_gets (vt_lexer_t *lxr)
{
  assert (lxr);

  return vt_buf_str (&lxr->value);
}

ssize_t
vt_lexer_putc (vt_lexer_t *lxr, char chr, vt_error_t *err)
{
  char chrs[2];

  assert (lxr);

  chrs[0] = chr;
  chrs[1] = '\0';

  if (! vt_buf_ncpy (&lxr->value, chrs, 2)) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    return -1;
  }

  return 1;
}

ssize_t
vt_lexer_puts (vt_lexer_t *lxr, char *str, size_t len, vt_error_t *err)
{
  assert (lxr);
  assert (str);

  if (! vt_buf_ncpy (&lxr->value, str, len)) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: %s", __func__, strerror (errno));
    return -1;
  }

  return len;
}

ssize_t
vt_lexer_clear (vt_lexer_t *lxr, vt_error_t *err)
{
  assert (lxr);

  return vt_lexer_putc (lxr, '\0', err);
}

#define vt_is_comment(c) ((c) == '#')
#define vt_is_escape(c) ((c) == '\\')
#define vt_is_equals_sign(c) ((c) == '=')
#define vt_is_newline(c) ((c) == '\n')
#define vt_is_option_first(c) (isalnum ((c)) || (c) == '_')
#define vt_is_option(c) (isalnum ((c)) || (c) == '-' || (c) == '_')
#define vt_is_title_first(c) (isalnum ((c)) || (c) == '_')
#define vt_is_title(c) (isalnum ((c)) || (c) == '-' || (c) == '_')
#define vt_is_left_curly(c) ((c) == '{')
#define vt_is_right_curly(c) ((c) == '}')

vt_lexer_token_t
vt_lexer_get_token (vt_lexer_t *lxr, vt_lexer_opts_t *opts, vt_error_t *err)
{
  int chr, delim, esc;
  size_t eof, first, last;
  vt_lexer_state_t next_state;

  assert (lxr);
  assert (opts);

  eof = 0;
  first = 0;
  last = 0;
  chr = '\0';
  delim = '\0';
  esc = 0;

  /*vt_error ("s:%d is none", VT_LEXER_STATE_NONE);
  vt_error ("s:%d is comment", VT_LEXER_STATE_COMMENT);
  vt_error ("s:%d is option", VT_LEXER_STATE_OPTION);
  vt_error ("s:%d is operator", VT_LEXER_STATE_OPERATOR);
  vt_error ("s:%d is value", VT_LEXER_STATE_VALUE);
  vt_error ("s:%d is title", VT_LEXER_STATE_TITLE);
  vt_error ("s:%d is section", VT_LEXER_STATE_SECTION);
  vt_error ("s:%d is delim", VT_LEXER_STATE_DELIM);
  vt_error ("s:%d is value_delim", VT_LEXER_STATE_VALUE_DELIM);
  vt_error ("s:%d is seperator", VT_LEXER_STATE_SEPERATOR);*/

  for (; ; lxr->pos++, lxr->colno++) {
    chr = lxr->str[lxr->pos];

    /* end of file */
    if (lxr->pos >= lxr->len || chr == '\0') {
      vt_error ("oops... EOF!");
      eof = 1;
    }
    /* update line and column counters */
    if (vt_is_newline (chr)) {
      lxr->colno = 1;
      lxr->lineno++;
    }

    //vt_error ("l:%d,c:%d,s:%d, chr: '%c'\n", lxr->lineno, lxr->colno, lxr->state, chr);

    if (lxr->state == VT_LEXER_STATE_NONE) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        return VT_TOKEN_EOF;
      /* alpha-numeric character marks start of name */
      } else if (vt_is_option_first (chr)) {
        first = lxr->pos;
        lxr->state = VT_LEXER_STATE_OPTION;
      /* left curly marks start of section */
      } else if (vt_is_left_curly (chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->pos++;
        return VT_TOKEN_LEFT_CURLY;
      /* right curly marks end of section */
      } else if (vt_is_right_curly (chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->pos++;
        return VT_TOKEN_RIGHT_CURLY;
      /* ignore comments */
      } else if (vt_is_comment (chr)) {
        next_state = VT_LEXER_STATE_NONE;
        lxr->state = VT_LEXER_STATE_COMMENT;
      /* ignore white space */
      } else if (! isspace (chr)) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_COMMENT) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        return VT_TOKEN_EOF;
      /* newline marks end of comment */
      } else if (vt_is_newline (chr)) {
        lxr->state = next_state;
      }

    } else if (lxr->state == VT_LEXER_STATE_OPTION) {
      /* non option class character indicates end of option */
      if (! vt_is_option (chr)) {
        last = lxr->pos;
        if (vt_lexer_puts (lxr, lxr->str + first, last - first, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_OPERATOR;
        return VT_TOKEN_OPTION;
      }

    } else if (lxr->state == VT_LEXER_STATE_OPERATOR) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        return VT_TOKEN_EOF;
      /* ignore comments */
      } else if (vt_is_comment (chr)) {
        next_state = VT_LEXER_STATE_NONE;
        lxr->state = VT_LEXER_STATE_COMMENT;
      /* equals sign '=' marks start of value */
      } else if (vt_is_equals_sign (chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_DELIM;
        lxr->pos++;
        return VT_TOKEN_OPERATOR;
      /* left curly marks start of section */
      } else if (vt_is_left_curly (chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_NONE;
        lxr->pos++;
        return VT_TOKEN_LEFT_CURLY;
      /* alpha-numeric characters mark start of title */
      } else if (vt_is_title_first (chr)) {
        first = lxr->pos;
        lxr->state = VT_LEXER_STATE_TITLE;
      } else if (! isspace (chr)) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_TITLE) {
      /* white space marks end of title */
      /* right curly marks end of title and start of section */
      if (eof || isspace (chr) || vt_is_left_curly (chr)) {
        last = lxr->pos - 1;
        if (vt_lexer_puts (lxr, lxr->str + first, last + 1 - first, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_SECTION;
        return VT_TOKEN_TITLE;
      /* validate characters in title */
      } else if (! vt_is_title (chr)) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_SECTION) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        return VT_TOKEN_EOF;
      /* left curly marks start of section */
      } else if (vt_is_left_curly (chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_NONE;
        lxr->pos++;
        return VT_TOKEN_LEFT_CURLY;
      /* ignore comments */
      } else if (vt_is_comment (chr)) {
        next_state = VT_LEXER_STATE_NONE;
        lxr->state = VT_LEXER_STATE_COMMENT;
      /* ignore white space */
      } else if (! isspace (chr)) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_DELIM) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        return VT_TOKEN_EOF;
      /* delimiter marks start of value */
      } else if (opts->delim && strchr (opts->delim, chr)) {
        first = lxr->pos + 1;
        delim = chr;
        lxr->state = VT_LEXER_STATE_VALUE_DELIM;
      /* any character in character class marks start of value */
      } else if (opts->class && strchr (opts->class, chr)) {
        first = lxr->pos;
        delim = '\0';
        lxr->state = VT_LEXER_STATE_VALUE;
      /* ignore comments */
      } else if (vt_is_comment (chr)) {
        next_state = VT_LEXER_STATE_NONE;
        lxr->state = VT_LEXER_STATE_COMMENT;
      /* ignore white space */
      } else if (! isspace (chr)) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_VALUE) {
      /* end of file marks end of value */
      /* white space marks end of value */
      if (eof || isspace (chr)) {
        last = lxr->pos - 1;
        if (vt_lexer_puts (lxr, lxr->str + first, last + 1 - first, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_SEPERATOR;
        return VT_TOKEN_VALUE;
      /* any character outside character class marks error */
      } else if (! (opts->class && strchr (opts->class, chr))) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      }

    } else if (lxr->state == VT_LEXER_STATE_VALUE_DELIM) {
      /* end of file */
      if (eof) {
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      /* delimeter marks end of value */
      } else if (! esc && chr == delim) {
        last = lxr->pos - 1;
        if (vt_lexer_puts (lxr, lxr->str + first, last + 1 - first, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_SEPERATOR;
        lxr->pos++;
        return VT_TOKEN_VALUE;
      /* escape character escapes delimiter */
      } else if (esc) {
        esc = 0;
      } else if (vt_is_escape (chr)) {
        esc = 1;
      }

    } else if (lxr->state == VT_LEXER_STATE_SEPERATOR) {
      /* end of file */
      if (eof) {
        if (vt_lexer_clear (lxr, err) < 0)
          return VT_TOKEN_ERROR;
        vt_set_error (err, VT_ERR_BADCFG);
        return VT_TOKEN_ERROR;
      /* newline marks end of value */
      } else if (vt_is_newline (chr)) {
        lxr->state = VT_LEXER_STATE_NONE;
      /* seperator indicates another value will be encountered */
      } else if (opts->sep && strchr (opts->sep, chr)) {
        if (vt_lexer_putc (lxr, chr, err) < 0)
          return VT_TOKEN_ERROR;
        lxr->state = VT_LEXER_STATE_DELIM;
        lxr->pos++;
        return VT_TOKEN_SEPERATOR;
      /* character in class name marks start of */
      } else if (vt_is_option_first (chr)) {
        first = lxr->pos;
        lxr->state = VT_LEXER_STATE_OPTION;
      /* ignore comments */
      } else if (vt_is_comment (chr)) {
        lxr->state = VT_LEXER_STATE_COMMENT;
      }
    } else {
      vt_panic ("this is weird!");
    }
  }

failure:
  return -1;
}

#undef vt_is_comment
#undef vt_is_escape
#undef vt_is_equals_sign
#undef vt_is_newline
#undef vt_is_option_first
#undef vt_is_option
#undef vt_is_left_curly
#undef vt_is_right_curly
