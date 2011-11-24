/* system includes */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* valiant includes */
#include "stage.h"

vt_stage_t *
vt_stage_create (const vt_map_list_t *list, cfg_t *sec, vt_error_t *err)
{
  int lerr = 0;
  vt_stage_t *stage;

  if (! (stage = calloc (1, sizeof (vt_stage_t)))) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }
  if (! (stage->maps = vt_map_lineup_create (list, sec, &lerr)) && lerr != 0) {
    vt_set_error (err, lerr);
    (void)vt_stage_destroy (stage, 0, NULL);
    return NULL;
  }

  return stage;
}

int
vt_stage_destroy_slist (void *ptr)
{
  return vt_stage_destroy ((vt_stage_t *)ptr, 1, NULL);
}

int
vt_stage_destroy (vt_stage_t *stage, int recurse, vt_error_t *err)
{
  // FIXME: IMPLEMENT
  return 0;
}

int
vt_stage_add_check (vt_stage_t *stage, const vt_check_t *check, vt_error_t *err)
{
  vt_slist_t *cur, *next;
  vt_check_t *p;

  /* avoid duplicates */
  for (cur=stage->checks; cur; cur=cur->next) {
    p = (vt_check_t *)cur->data;

    if (strcmp (check->name, p->name) == 0) {
      vt_set_error (err, VT_ERR_ALREADY);
      vt_error ("%s: check %s already included", __func__, check->name);
      return -1;
    }
  }

  if ((next = vt_slist_append (stage->checks, (void *)check)) == NULL) {
    vt_set_error (err, VT_ERR_NOMEM);
    vt_error ("%s: slist_append: %s", __func__, strerror (errno));
    return -1;
  }

  if (stage->checks == NULL)
    stage->checks = next;

  return 0;
}

void
vt_stage_update (vt_stage_t *stage)
{
  vt_check_t *check;
  vt_slist_t *entry;
  int max, min;

  max = min = 0;

  vt_slist_sort (stage->checks, &vt_check_sort);

  for (entry=stage->checks; entry; entry=entry->next) {
    if (entry->data) {
      check = (vt_check_t *)entry->data;
      max += check->max_weight;
      min += check->max_weight;
    }
  }

  stage->max_weight = max;
  stage->min_weight = min;
}
