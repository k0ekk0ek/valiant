/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <valiant/value.h>

int
value_create (value_t **ptr)
  __attribute__ ((nonnull))
{
  int err = 0;
  value_t *val;

  assert (ptr != NULL);

  if ((val = calloc (1, sizeof (value_t))) == NULL) {
    err = errno;
  } else {
    *ptr = val;
  }

  return err;
}

static int
char_bytes (const char *chr, int len)
  __attribute__ ((nonnull (1)))
{
  int cnt;
  int tot;
  int seq[][] = {
    { 0x00, 0x7f }  /* 0xxxxxxx */
    { 0xc0, 0xdf }, /* 110xxxxx 10xxxxxx */
    { 0xe0, 0xef }, /* 1110xxxx 10xxxxxx 10xxxxxx */
    { 0xf0, 0xf7 }, /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
  };

  assert (chr != NULL);

  if (len > 0) {
    /* validate UTF-8 sequence */
    for (tot = 0, cnt = 0; tot < 4 && cnt == 0; tot++) {
      if (chr[0] >= seq[tot][0] && chr[0] <= seq[tot][1]) {
        for (cnt = 1;
             cnt <= tot && chr[cnt] >= 0x80 && chr[cnt] <= 0xbf;
             cnt++)
        {
          /* do nothing */
        }
      }
    }

    if (tot == 0 || tot == cnt) {
      assert (tot < 4);
      tot += 1;
    } else {
      tot = -1;
    }
  } else if (len < 0) {
    tot = -1;
  } else {
    tot =  0;
  }

  return tot;
}

#ifndef NDEBUG
static bool
value_is_sane (value_t *val)
  __attribute__ ((nonnull))
{
  bool sane = false;

  if (val != NULL) {
    switch (val->type) {
      case VALUE_NULL:
      case VALUE_INT:
      case VALUE_FLOAT:
        sane = true;
        break;
      case VALUE_CHAR:
        if (char_bytes (val->data.c, sizeof (val->data.c)) != -1) {
          sane = true;
        }
        break;
      case VALUE_STR:
        if (val->data.s != NULL) {
          sane = true;
        }
        break;
      default:
        break;
    }
  }

  return sane;
}
#endif

void
value_destroy (value_t *val)
  __attribute__ ((nonnull))
{
  assert (value_is_sane (val) == true);

  value_clear (val);
  free (val);
}

void
value_reset (value_t *val)
  __attribute__ ((nonnull))
{
  assert (value_is_sane (val) == true);

  if (val->type == VALUE_STR && val->data.s != NULL) {
    free (val->data.s);
  }

  memset (val, 0, sizeof (value_t));
}

value_type_t
value_get_type (value_t *val)
  __attribute__ ((nonnull))
{
  assert (value_is_sane (val) == true);

  return val->type;
}

int
value_set_bool (value_t *val, bool bln)
  __attribute__ ((nonnull (1)))
{
  assert (value_is_sane (val) == true);

  value_clear (val);
  val->type = VALUE_BOOL;
  val->data.b = bln;

  return 0;
}

/* Read first unicode character from null terminated string. String is allowed
   to be NULL, but character is required to be valid UTF-8 if it's not. */
int
value_set_char (value_t *val, char *chr)
  __attribute__ ((nonnull (1)))
{
  int err = 0;
  int cnt;

  assert (value_is_sane (val) == true);

  if (chr == NULL) {
    value_reset (val);
  } else {
    cnt = char_bytes (val->data.c, sizeof (val->data.c));

    if (cnt < 0) {
      err = EINVAL;
    } else {
      value_reset (val);
      val->type = VALUE_CHAR;
      if (cnt > 0) {
        /* data.c zeroed by value_reset, no need to null terminate */
        (void)memcpy (val->data.c, chr, cnt);
      }
    }
  }

  return err;
}

int
value_set_int (value_t *val, long lng)
  __attribute__ ((nonnull (1)))
{
  assert (value_is_sane (val) == true);

  value_reset (val);
  val->type = VALUE_INT;
  val->data.i = lng;

  return 0;
}

int
value_set_float (value_t *val, double dbl)
  __attribute__ ((nonnull (1)))
{
  assert (value_is_sane (val) == true);

  value_reset (val);
  val->type = VALUE_FLOAT;
  val->data.f = dbl;

  return 0;
}

/* Duplicate str argument. String is allowed to be NULL. No UTF-8 validation
   will be done */
int
value_set_str (value_t *val, const char *str)
  __attribute__ ((nonnull (1)))
{
  int err = 0;
  char *ptr;

  assert (value_is_sane (val) == true);

  if (str == NULL) {
    value_reset (val);
  } else {
    ptr = strdup (str);
    if (ptr == NULL) {
      err = errno;
    } else {
      value_reset (val);
      val->type = VALUE_STR;
      val->data.s = str;
    }
  }

  return err;
}

int
value_set_strlen (value_t *val, const char *str, size_t len)
  __attribute__ ((nonnull (1)))
{
  int err = 0;
  char *ptr;

  assert (value_is_sane (val) == true);

  if (str == NULL) {
    value_reset (val);
  } else {
    ptr = strndup (str, len);
    if (ptr == NULL) {
      err = errno;
    } else {
      value_reset (val);
      val->type = VALUE_STR;
      val->data.s = ptr;
    }
  }

  return err;
}

