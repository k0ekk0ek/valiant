/* system includes */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_str.h"
#include "utils.h"


check_str_t *
check_str_alloc (void)
{
  check_str_t *chk_str;

  if ((chk_str = malloc (sizeof (check_str_t))))
    memset (chk_str, 0, sizeof (check_str_t));

  return chk_str;
}


int
check_str_static (check_t *check, request_t *request, score_t *score)
{
	bool match;
	char *left, *right;
	check_str_t *extra;

	// XXX: insert assertion
	// XXX: only ASCII supported for now

	extra = (check_str_t *) check->extra;

	left = request_member (request, extra->left); // bails
	right = extra->right;

  g_printf ("%s: left: %s\nright: %s\n", __func__, left, right);

	if (extra->fold) {
		match = strcasecmp (left, right) ? false : true;

		//free (left); // XXX: left should never be freed
		//free (right);

	} else {
		match = strcmp (left, right) ? false : true;
	}

	return match ? check->plus : check->minus;
}


int
check_str_dynamic (check_t *check, request_t *request, score_t *score)
{
  bool match = true;
  char c1, c2;
  char *left, *right;
  char *p, *q, *t;
  int i;
	size_t n;
	check_str_t *extra;

  // XXX: insert assertion
  // XXX: only ASCII supported for now

  extra = (check_str_t *) check->extra;

  left = request_member (request, extra->left);
  right = extra->right;

  for (p=left, q=right; match && *p && *q; p++, q++) {
    if (q[0] == '%') {
      q++;

      if (q[0] != '%') {
        for (i=0; isdigit (q[0]); q++) {
          i = (i*10) + (q[0] - '0');
        }

        if (q[0] != '%')
          panic ("%s: unsuspected end of attribute", __func__);

        t = request_member (request, i);
        n = strlen (t);

        if (strncasecmp (p, t, n))
          match = false;
        else
          p += (n-1);

      } else if (p[0] != '%') {
        match = false;
      }

    } else {
      c1 = tolower (p[0]);
      c2 = tolower (q[0]);

      if (c1 != c2)
        match = false;
    }
  }

  return (match && p[0] == q[0]) ? check->plus : check->minus;
}


void
check_str_free (check_t *check)
{
  check_str_t *chk_str;

  if (check) {
    if (check->extra) {
      chk_str = (check_str_t *) check->extra;

      if (chk_str->right)
        free (chk_str->right);
      free (chk_str);
    }

    free (check);
  }

  return;
}


check_t *
check_str_create (int plus, int minus, char *left, char *right, bool fold)
{
  check_t *chk;
  check_str_t *chk_str;

	bool dynamic = false;
	char *p, *q, *s;
  int i;
  size_t n;

  /* check if both left and right are set, but only during debug */
  assert (left);
  assert (right);

  if (! (chk = check_alloc ()))
    bail ("%s: check_alloc: %s", __func__, strerror (errno));
  if (! (chk_str = check_str_alloc ()))
    bail ("%s: check_str_alloc: %s", __func__, strerror (errno));


  /* check if dynamic string compare */
  for (p=right; *p && ! dynamic; p++) {
    if (*(p) == '%' && *(p+1) != '%')
      dynamic = true;
  }

  n = (size_t) strlen (right);

  if (! (s = malloc (n+1)))
    panic ("%s: malloc: %s", __func__, strerror (errno));

  chk->plus = plus;
  chk->minus = minus;
  chk->extra = (void *) chk_str;
  chk->free_fn = &check_str_free;
  chk_str->fold = fold ? true : false;
  chk_str->left = request_member_id (left);
  chk_str->right = s;

  if (dynamic) {
    /*
     * Dynamic string compare. Replace attribute name with numerical
     * identifier to speed up the process at runtime.
     */
    for (p=right, q=NULL; *p; ) {
      if (q) {
        if (*p == '%') {
          n = (size_t) (p - q);
          i = request_member_id_len (q, n);

          if (i < 0)
            panic ("%s: invalid attribute %*s", __func__, n, q);
          if (sprintf (s, "%%%i%%", i) < 0)
            bail ("%s: g_sprintf: %s", __func__, strerror (errno));

          q = NULL;
        }
      
      } else {
        if (*p == '%' && *(p+1) && *(p+1) != '%')
          q = ++p;
        else
          *s++ = *p++;
      }
    }

    chk->check_fn = &check_str_dynamic;

  } else {
    /*
     * Static string compare. Remove escaped percent signs and assign static
     * string comparison function.
     */
    for (p=right; *p; p++) {
      if (*(p) == '%' && *(p+1) != '\0')
      	p++;

      *s++ = *p;
    }

    chk->check_fn = &check_str_static;
  }

	return chk;
}


/**
 * COMMENT
 */
check_t *
cfg_to_check_str (cfg_t *section)
{
  check_t *check;

  char *attribute;
  char *format;
  int positive;
  int negative;
  bool fold;

  if (! (attribute = cfg_getstr (section, "attribute")))
    panic ("%s: attribute undefined\n", __func__);
  if (! (format = cfg_getstr (section, "format")))
    panic ("%s: format undefined\n", __func__);

  attribute = strdup (attribute);
  format = strdup (format);

  positive = (int) (cfg_getfloat (section, "positive") * 100);
  negative = (int) (cfg_getfloat (section, "negative") * 100);
  fold = cfg_getbool (section, "case-sensitive");

  g_printf ("%s: attribute: %s, format: %s, positive: %d, negative: %d, fold: %s\n",
  __func__,
  attribute,
  format,
  positive,
  negative,
  fold ? "true" : "false");

  return check_str_create (positive, negative, attribute, format, fold);
}
























































