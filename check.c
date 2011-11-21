/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "error.h"
#include "check.h"
#include "check_priv.h"
#include "map.h"

vt_check_t *
vt_check_create (const vt_map_list_t *list, cfg_t *sec, vt_errno_t *err)
{
  char *title;
  int ret;
  vt_check_t *check = NULL;

  if ((title = (char *)cfg_title (sec)) == NULL) {
    vt_set_errno (err, VT_ERR_BADCFG);
    vt_error ("%s (%d): missing option 'title' for section 'check'");
    goto FAILURE;
  }
  if (! (check = calloc (1, sizeof (vt_check_t)))) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
  }
  if (! (check->name = strdup (title))) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): strdup: %s", __func__, __LINE__, strerror (errno));
    goto FAILURE;
  }
  if (! (check->maps = vt_map_lineup_create (list, sec, &ret))) {
    vt_set_errno (err, ret);
    goto FAILURE;
  }

  return check;
FAILURE:
  vt_check_destroy (check);
  return NULL;
}

int
vt_check_destroy (vt_check_t *check, vt_errno_t **err)
{
  if (check) {
    if (check->data)
      free (check->data);
    if (check->maps)
      free (check->maps);
    free (check);
  }
  return 0;
}

int
vt_check_sort (void *p1, void *p2)
{
  vt_check_t *c1, *c2;

  c1 = (vt_check_t *)p1;
  c2 = (vt_check_t *)p2;

  if (c1->prio > c2->prio)
    return -1;
  if (c1->prio < c2->prio)
    return  1;

  return 0;
}

int
vt_check_dynamic_pattern (const char *s)
{
  char *p;

  for (p=(char*)s; *p; p++) {
    if (*p == '%' && *++p != '%')
      return 1;
  }

  return 0;
}

char *
vt_check_unescape_pattern (const char *s1)
{
  char *s2, *p1, *p2;
  size_t n;

  n = strlen (s1);

  if ((s2 = calloc (1, n + 1)) == NULL)
    return NULL;

  for (p1=(char*)s1, p2=s2; *p1;) {
    *p2++ = *p1;

    if (*p1++ == '%' && *p1 == '%')
      ++p1;
  }

 *p2 = '\0';

  return s2;
}

