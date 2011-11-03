/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "valiant.h"
#include "check.h"
#include "map.h"
#include "utils.h"

int
vt_check_create (vt_check_t **dest, size_t size, const vt_map_list_t *list,
  const cfg_t *sec)
{
  char *title;
  int ret, err;
  vt_check_t *check = NULL;
  vt_map_id_t *maps = NULL;

  if ((title = (char *)cfg_title ((cfg_t *)sec)) == NULL) {
    vt_error ("%s: check title unavailable", __func__);
    err = VT_ERR_BADCFG;
    goto FAILURE;
  }
  if ((check = malloc0 (sizeof (vt_check_t))) == NULL ||
      (size && (check->data = malloc0 (size)) == NULL))
  {
    vt_error ("%s: malloc0: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if ((check->name = strdup (title)) == NULL) {
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    err = VT_ERR_NOMEM;
    goto FAILURE;
  }
  if ((ret = vt_map_ids_create (&maps, list, sec)) != VT_SUCCESS) {
    err = ret;
    goto FAILURE;
  }

  check->maps = maps;
  *dest = check;
  return VT_SUCCESS;
FAILURE:
  vt_check_destroy (check);
  return err;
}

void
vt_check_destroy (vt_check_t *check)
{
  if (check) {
    if (check->data)
      free (check->data);
    free (check);
  }
}

int
vt_check_weight (float weight)
{
  return (int)(weight*100);
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

  if ((s2 = malloc (n+1)) == NULL)
    return NULL;

  for (p1=(char*)s1, p2=s2; *p1;) {
    *p2++ = *p1;

    if (*p1++ == '%' && *p1 == '%')
      ++p1;
  }

 *p2 = '\0';

  return s2;
}
