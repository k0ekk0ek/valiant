#ifndef VT_STAGE_H_INCLUDED
#define VT_STAGE_H_INCLUDED

/* system includes */
#include <confuse.h>
#include <stdbool.h>

/* valiant includes */
#include "check.h"
#include "map.h"
#include "slist.h"

typedef struct vt_stage_struct vt_stage_t;

struct vt_stage_struct {
  int min_weight; /* minimum score for all checks in this stage */
  int max_weight; /* maximum socre fro all checks in this stage */
  vt_map_id_t *maps;
  vt_slist_t *checks;
};

int vt_stage_create (vt_stage_t **dest, const vt_map_list_t *, const cfg_t *);
void vt_stage_destroy (vt_stage_t *, bool);
int vt_stage_set_check (vt_stage_t *, const vt_check_t *);
int vt_stage_lineup (vt_stage_t *);
int vt_stage_enter (vt_stage_t *, vt_request_t *, vt_score_t *, vt_stats_t *,
  vt_map_list_t *);

#endif

