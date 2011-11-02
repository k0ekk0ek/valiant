#ifndef VT_STAGE_H_INCLUDED
#define VT_STAGE_H_INCLUDED

/* system includes */

/* valiant includes */
#include "map.h"
#include "slist.h"

typedef struct vt_stage_struct vt_stage_t;

struct vt_stage_struct {
  int min_weight; /* minimum score for all checks in this stage */
  int max_weight; /* maximum socre fro all checks in this stage */
  int *include;
  int *exclude;
  slist_t *checks;
};

int vt_stage_create (vt_stage_t **);
void vt_stage_destroy (vt_stage_t *, int);
int vt_stage_insert_check (vt_stage_t *, const vt_check_t *);
int vt_stage_update_weights (vt_stage_t *);
int vt_stage_do (vt_stage_t *, vt_request_t *, vt_score_t *, vt_stats_t *,
  vt_set_t *);

#endif

