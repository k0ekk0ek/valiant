/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "vt_value.h"

vt_value_t *
vt_value_create (vt_value_type_t type, ...)
{
  va_list ap;
  vt_value_t *val;

  if (! (calloc (1, sizeof (vt_value_t)))) {
    //
  }

  return val;
}

void
vt_value_destroy (vt_value_t *val)
{
  if (val) {
    if (val->type == VT_VALUE_TYPE_STR && val->data.str)
      free (val->data.str);
    free (val);
  }
}

int
vt_value_cmp (vt_value_t *val1, vt_value_t *val2, int *err)
{
  int ret = 0;

  if (! val1) {
    ret = (val2) ? -1 : 0;
    goto error;
  }
  if (! val2) {
    ret = 1;
    goto error;
  }
  if (val1->type > val2->type) {
    ret = 1;
    goto error;
  }
  if (val1->type < val2->type) {
    ret = -1;
    goto error;
  }

  switch (val1->type) {
    case VT_VALUE_TYPE_BOOL:
      if (val1->data.bln && ! val2->data.bln)
        ret = 1;
      else if (val2->data.bln && ! val1->data.bln)
        ret = -1;
      break;
    case VT_VALUE_TYPE_INT:
      if (val1->data.lng > val2->data.lng)
        ret = 1;
      if (val1->data.lng < val2->data.lng)
        ret = -1;
      break;
    case VT_VALUE_TYPE_FLOAT:
      if (val1->data.dbl > val2->data.dbl)
        ret = 1;
      if (val1->data.dbl < val2->data.dbl)
        ret = -1;
      break;
    case VT_VALUE_TYPE_STR:
      if (! val1->data.str) {
        ret = (val2->data.str) ? -1 : 0;
        goto error;
      }
      if (! val2->data.str) {
        ret = 1;
        goto error;
      }
      ret = strcmp (val1->data.str, val2->data.str);
      break;
  }

  return ret;
error:
  vt_set_errno (err, EINVAL);
  return ret;
}

vt_value_t *
vt_value_dup (vt_value_t *src)
{
  vt_value_t *dest;

  assert (src);

  if (! (dest = calloc (1, sizeof (vt_value_t))))
    goto error;

  switch (src->type) {
    case VT_VALUE_TYPE_BOOL:
      dest->type = VT_VALUE_TYPE_BOOL;
      dest->data.bln = src->data.bln;
      break;
    case VT_VALUE_TYPE_INT:
      dest->type = VT_VALUE_TYPE_INT;
      dest->data.lng = src->data.lng;
      break;
    case VT_VALUE_TYPE_FLOAT:
      dest->type = VT_VALUE_TYPE_FLOAT;
      dest->data.dbl = src->data.dbl;
      break;
    case VT_VALUE_TYPE_STR:
      dest->type = VT_VALUE_TYPE_STR;
      if (src->data.str && ! (dest->data.str = strdup (src->data.str)))
        goto error;
      break;
  }

  return dest;
error:
  if (dest)
    free (dest);
  return NULL;
}

