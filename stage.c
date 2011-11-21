/* system includes */
#include <errno.h>
#include <string.h>

/* valiant includes */
#include "error.h"
#include "slist.h"
#include "stage.h"

vt_stage_t *
vt_stage_init (const vt_map_list_t *list, cfg_t *sec, vt_errno_t *err)
{
  int ret = 0;
  vt_stage_t *stage;

  if (! (stage = calloc (1, sizeof (vt_stage_t)))) {
    vt_set_errno (err, VT_ERR_NOMEM);
    vt_error ("%s (%d): calloc: %s", __func__, __LINE__, strerror (errno));
    return NULL;
  }
  if (! (stage->maps = vt_map_lineup_create (list, sec, &ret)) && ret != 0) {
    vt_set_errno (err, ret);
    vt_stage_destroy (stage, 0);
    return NULL;
  }

  return stage;
}

void
vt_stage_destroy (vt_stage_t *stage, int recurse)
{
  // IMPLEMENT
}

int
vt_stage_add_check (vt_stage_t *stage, const vt_check_t *check, vt_errno_t *err)
{
  vt_slist_t *cur, *next;
  vt_check_t *p;

  /* avoid duplicates */
  for (cur=stage->checks; cur; cur=cur->next) {
    p = (vt_check_t *)cur->data;

    if (strcmp (check->name, p->name) == 0) {
      vt_set_errno (err, VT_ERR_ALREADY);
      vt_error ("%s: check %s already included", __func__, check->name);
      return -1;
    }
  }

  if ((next = vt_slist_append (stage->checks, check)) == NULL) {
    vt_set_errno (err, VT_ERR_NOMEM);
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
      max += check->weight_func (check, 1);
      min += check->weight_func (check, 0);
    }
  }

  stage->max_weight = max;
  stage->min_weight = min;
}
