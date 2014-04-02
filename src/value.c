/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unitypes.h>
#include <unistr.h>

/* valiant includes */
#include <valiant/value.h>

int
value_create (value_t **a_val)
{
  int err = 0;
  value_t *val;

  assert (a_val != NULL);

  if ((val = calloc (1, sizeof (value_t))) == NULL) {
    err = errno;
  } else {
    *a_val = val;
  }

  return err;
}

#if !defined NDEBUG
static int
value_is_sane (value_t *val)
{
  int err = EINVAL;
  size_t len;

  if (val != NULL) {
    switch (val->type) {
      case VALUE_NULL:
      case VALUE_BOOL:
      case VALUE_CHAR:
      case VALUE_LONG:
      case VALUE_DOUBLE:
        err = 0;
        break;
      case VALUE_STR:
        if (val->data.str != NULL) {
          len = strlen ((char *)val->data.str);
          if (u8_check (val->data.str, len) == NULL) {
            err = 0;
          }
        }
        break;
      default:
        break;
    }
  }

  return err;
}
#endif

void
value_destroy (value_t *val)
{
  if (val != NULL) {
    assert (value_is_sane (val) == 0);
    value_clear (val);
    free (val);
  }
}

void
value_clear (value_t *val)
{
  assert (value_is_sane (val) == 0);

  if (val->type == VALUE_STR && val->data.str != NULL) {
    free (val->data.str);
  }

  memset (val, 0, sizeof (value_t));
}

int
value_copy (value_t *a_val, value_t *val)
{
  int err = 0;
  uint8_t *str;

  assert (value_is_sane (val) == 0);

  if (val->type == VALUE_STR) {
    str = (uint8_t *)strdup ((char *)val->data.str);
    if (str != NULL) {
      a_val->type = val->type;
      a_val->data.str = str;
    } else {
      err = errno;
    }
  } else {
    (void)memcpy (a_val, val, sizeof (value_t));
  }

  return err;
}

value_type_t
value_get_type (value_t *val)
{
  assert (value_is_sane (val) == 0);

  return val->type;
}

int
value_get_bool (bool *bln, value_t *val)
{
  assert (bln != NULL);
  assert (value_is_sane (val) == 0);

  return val->type == VALUE_BOOL ? *bln = val->data.bln, 0 : EINVAL;
}

void
value_set_bool (value_t *val, bool bln)
{
  assert (value_is_sane (val) == 0);

  value_clear (val);
  val->type = VALUE_BOOL;
  val->data.bln = bln;
}

int
value_get_char (ucs4_t *chr, value_t *val)
{
  assert (chr != NULL);
  assert (value_is_sane (val) == 0);

  return val->type == VALUE_CHAR ? *chr = val->data.chr, 0 : EINVAL;
}

void
value_set_char (value_t *val, ucs4_t chr)
{
  assert (value_is_sane (val) == 0);

  value_clear (val);
  val->type = VALUE_CHAR;
  val->data.chr = chr;
}

int
value_get_long (long *lng, value_t *val)
{
  assert (lng != NULL);
  assert (value_is_sane (val) == 0);

  return val->type == VALUE_LONG ? *lng = val->data.lng, 0 : EINVAL;
}

void
value_set_long (value_t *val, long lng)
{
  assert (value_is_sane (val) == 0);

  value_clear (val);
  val->type = VALUE_LONG;
  val->data.lng = lng;
}

int
value_get_double (double *dbl, value_t *val)
{
  assert (dbl != NULL);
  assert (value_is_sane (val) == 0);

  return val->type == VALUE_DOUBLE ? *dbl = val->data.dbl, 0 : EINVAL;
}

void
value_set_double (value_t *val, double dbl)
{
  assert (value_is_sane (val) == 0);

  value_clear (val);
  val->type = VALUE_DOUBLE;
  val->data.dbl = dbl;
}

int
value_get_str (uint8_t **str, value_t *val)
{
  int err = 0;
  uint8_t *ptr;

  assert (str != NULL);
  assert (value_is_sane (val) == 0);

  if (val->type == VALUE_STR) {
    ptr = (uint8_t *)strdup ((char *)val->data.str);
    if (ptr != NULL) {
      *str = ptr;
    } else {
      err = errno;
    }
  } else {
    err = EINVAL;
  }

  return err;
}

int
value_set_str (value_t *val, const uint8_t *str, size_t len)
{
  int err = 0;
  uint8_t *ptr;

  assert (value_is_sane (val) == 0);
  assert (str != NULL);

  if (len == 0 || len == SIZE_MAX) {
    ptr = (uint8_t *)strdup ((char *)str);
  } else {
    ptr = (uint8_t *)strndup ((char *)str, len);
  }

  if (ptr != NULL) {
    value_clear (val);
    val->type = VALUE_STR;
    val->data.str = (uint8_t *)ptr;
  } else {
    err = errno;
  }

  return err;
}
