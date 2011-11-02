#ifndef VT_CHECK_H_INCLUDED
#define VT_CHECK_H_INCLUDED

/* system includes */
#include <confuse.h>
#include <sys/types.h>

/* valiant includes */
#include "map.h"
#include "request.h"
#include "score.h"
#include "stats.h"

typedef struct vt_check_struct vt_check_t;

typedef int(*VT_CHECK_CHECK_FUNC)(vt_check_t *, vt_request_t *, vt_score_t *, 
  vt_stats_t *);
typedef int(*VT_CHECK_WEIGHT_FUNC)(vt_check_t *, int);
typedef void(*VT_CHECK_DESTROY_FUNC)(vt_check_t *);

struct vt_check_struct {
  char *name; /* check title */
  unsigned int prio; /* indicates check cost */
  void *data; /* check specific information */
  vt_map_id_t *maps;
  VT_CHECK_CHECK_FUNC check_func;
  VT_CHECK_WEIGHT_FUNC weight_func;
  VT_CHECK_DESTROY_FUNC destroy_func;
};

#define VT_CHECK_TYPE_FLAG_NONE (0)
#define VT_CHECK_TYPE_FLAG_NEED_INIT (1<<1)

typedef struct vt_check_type_struct vt_check_type_t;

typedef int(*VT_CHECK_TYPE_INIT_FUNC)(const cfg_t *);
typedef int(*VT_CHECK_TYPE_DEINIT_FUNC)(void);
typedef int(*VT_CHECK_TYPE_CREATE_CHECK_FUNC)(vt_check_t **, const vt_map_list_t *, const cfg_t *);

struct vt_check_type_struct {
  char *name;
  unsigned int flags;
  VT_CHECK_TYPE_INIT_FUNC init_func;
  VT_CHECK_TYPE_DEINIT_FUNC deinit_func;
  VT_CHECK_TYPE_CREATE_CHECK_FUNC create_check_func;
};

/* DO NOT USE, use create_check_func member */
int vt_check_create (vt_check_t **, size_t, const vt_map_list_t *, const cfg_t);
/* DO NOT USE, use destroy_func member */
void vt_check_destroy (vt_check_t *);
int vt_check_weight (float);
int vt_check_sort (void *, void *);
int vt_check_dynamic_pattern (const char *);
char *vt_check_unescape_pattern (const char *);

#endif
