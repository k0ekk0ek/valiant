#ifndef VT_STAGE_H_INCLUDED
#define VT_STAGE_H_INCLUDED 1

/* valiant includes */
#include "check.h"
#include "slist.h"

typedef struct vt_stage_struct vt_stage_t;

struct vt_stage_struct {
  int min_weight;
  int max_weight;
  int min_weight_diff;
  int max_weight_diff;
  int *maps;
  vt_slist_t *checks;
};

vt_stage_t *vt_stage_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
int vt_stage_destroy_slist (void *);
int vt_stage_destroy (vt_stage_t *, int, vt_error_t *);
int vt_stage_add_check (vt_stage_t *, const vt_check_t *, vt_error_t *);
void vt_stage_update (vt_stage_t *);

#endif
