#ifndef VT_STAGE_H_INCLUDED
#define VT_STAGE_H_INCLUDED 1

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"
#include "error.h"
#include "map.h"
#include "slist.h"

typedef struct vt_stage_struct vt_stage_t;

struct vt_stage_struct {
  int min_weight; /* minimum score for all checks in this stage */
  int max_weight; /* maximum socre fro all checks in this stage */
  int min_gained_weight;
  int max_gained_weight;
  vt_map_id_t *maps;
  vt_slist_t *checks;
};

vt_stage_t *vt_stage_create (const vt_map_list_t *, const cfg_t *);
void vt_stage_destroy (vt_stage_t *, int);
int vt_stage_add_check (vt_stage_t *, const vt_check_t *);
void vt_stage_update (vt_stage_t *);

#endif
