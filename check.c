/* system includes */
#include <confuse.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "check_priv.h"
#include "conf.h"

vt_check_t *
vt_check_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  vt_check_t *check;
  vt_error_t lerr;

  if (! (check = calloc (1, sizeof (vt_check_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: calloc: %s", __func__, strerror (errno));
    goto FAILURE;
  }
  if (! (check->name = vt_cfg_titledup (sec))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: strdup: %s", __func__, strerror (errno));
    goto FAILURE;
  }
vt_error ("%s (%d)", __func__, __LINE__);
  lerr = 0;
  if (! (check->maps = vt_map_lineup_create (list, sec, &lerr)) && lerr != 0) {
    vt_set_error (err, lerr);
    goto FAILURE;
  }
vt_error ("%s (%d)", __func__, __LINE__);
  return check;
FAILURE:
  (void)vt_check_destroy (check, NULL);
  return NULL;
}

int
vt_check_destroy (vt_check_t *check, vt_error_t *err)
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

int
vt_check_weight (float weight)
{
  return (int)(weight * 100);
}

int
vt_check_types_init (cfg_t *cfg, vt_check_type_t **types, vt_error_t *err)
{
  cfg_t *sec;
  char *type;
  int i, j, n;
  int ntypes;
  vt_error_t lerr;

  for (i = 0, ntypes = 0; types[i]; i++) {
    if (types[i]->init_func)
      ntypes++;
  }

  for (i = 0, n = cfg_size (cfg, "type"); i < n; i++) {
    if ((sec = cfg_getnsec (cfg, "type", i)) &&
        (type = (char *)cfg_title (sec)))
    {
      for (j = 0; types[j]; j++) {
        if (types[j]->init_func && strcmp (type, types[j]->name) == 0) {
          if (types[j]->init_func (sec, &lerr) < 0) {
            vt_set_error (err, lerr);
            goto FAILURE;
          }
          break;
        }
      }

      if (! types[j]) {
        vt_set_error (err, VT_ERR_BADCFG);
        vt_error ("%s: bad check type %s", __func__, type);
        goto FAILURE;
      }
    }
  }

  if (i != ntypes) {
    vt_set_error (err, VT_ERR_BADCFG);
    vt_error ("%s: some check types were not initialize", __func__);
    goto FAILURE;
  }

  return 0;
FAILURE:
  // free, memory, deinitialize stuff!
  return -1;
}
