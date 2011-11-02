/* system includes */
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* valiant includes */
#include "slist.h"
#include "stage.h"
#include "utils.h"
#include "valiant.h"

int
vt_stage_create (vt_stage_t **dest, const vt_map_list_t *list, const cfg_t *sec)
{
  int ret;
  vt_map_id_t *maps = NULL;
  vt_stage_t *stage = NULL;

  if ((stage = malloc0 (sizeof (vt_stage_t))) == NULL) {
    return VT_ERR_NOMEM;
  }
  if ((ret = vt_map_ids_create (&maps, list, sec)) != VT_SUCCESS) {
    free (stage);
    return err;
  }

  return VT_SUCCESS;
}

void
vt_stage_destroy (vt_stage_t *stage, bool checks)
{
  // IMPLEMENT
}

int
vt_stage_insert_check (vt_stage_t *stage, vt_check_t *check)
{
  vt_slist_t *cur, *next;

  for (cur=stage->checks; cur; cur=cur->next) {
    if (strcmp (check->name, cur->name) == NULL) {
      vt_error ("%s: check %s already included", __func__, check->name);
      return VT_ERR_ALREADY;
    }
  }

  if ((next = vt_slist_append (stage->checks, check)) == NULL) {
    vt_error ("%s: slist_append: %s", __func__, strerror (errno));
    return VT_ERR_NOMEM;
  }

  if (stage->checks == NULL) {
    stage->checks = next;
  }

  return VT_SUCCESS;
}

int
vt_stage_update_weights (vt_stage_t *stage)
{
  vt_check_t *check;
  vt_slist_t *entry;
  int max, min;

  max = min = 0;

  for (entry=stage->checks; entry; entry=entry->next) {
    if (entry->data) {
      check = (vt_check_t *)entry->data;
      max += check->weight_func (check, 1);
      min += check->weight_func (check, 0);
    }
  }

  stage->max_weight = max;
  stage->min_weight = min;

  return VT_SUCCESS;
}

int
vt_stage_do (vt_stage_t *stage,
             vt_request_t *request,
             vt_score_t *score,
             vt_stats_t *stats,
             vt_set_t *set)
{
  int res, err;
  vt_check_t *check;
  vt_slist_t *cur;

  err = VT_SUCCESS;

  /* check if this stage should be entered for the current request */
  ret = vt_set_do (&res, set, stage->include, stage->exclude, request);
  if (ret != VT_SUCCESS)
    return ret;
  if (! exec)
    return VT_SUCCESS;

  for (cur=stage->checks; cur; cur=cur->next) {
    check = (vt_check_t *)cur->data;
    ret = vt_set_do (&res, set, check->include, stage->exclude, request);
    if (ret != VT_SUCCESS) {
      err = ret;
      break;
    }
    if (res) {
      ret = check->check_func (check, request, score, stats);
      if (ret != VT_SUCCESS) {
        err = ret;
        break;
      }
    }
  }

  vt_score_wait (score);

  return err;
}
