#ifndef VT_STAGE_H_INCLUDED
#define VT_STAGE_H_INCLUDED 1

/* valiant includes */
#include "check.h"
#include "slist.h"

typedef struct _vt_stage vt_stage_t;

struct _vt_stage {
  int min_diff;
  int max_diff;
  int min_chain_diff;
  int max_chain_diff;
  vt_slist_t *checks;
  int *requires;
};

//vt_stage_t *vt_stage_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
int vt_stage_destroy_slist (void *);
int vt_stage_destroy (vt_stage_t *, int, vt_error_t *);
int vt_stage_add_check (vt_stage_t *, const vt_check_t *, vt_error_t *);
void vt_stage_update (vt_stage_t *);

#endif
