#ifndef CHECK_H_INCLUDED
#define CHECK_H_INCLUDED

/* system includes */
#include <sys/types.h>

/* valiant includes */
#include "request.h"
#include "score.h"
#include "stats.h"

typedef struct vt_check_struct vt_check_t;

typedef int(*VT_CHECK_CHECK_FUNC)(vt_check_t *, vt_request_t *, vt_score_t *, 
  vt_stats_t *);
typedef int(*VT_CHECK_DESTROY_FUNC)(vt_check_t *);

struct vt_check_struct {
  int prio; /* indicates check cost */
  char *name; /* check title */
  void *info; /* check specific information */
  VT_CHECK_CHECK_FUNC check_func;
  VT_CHECK_DESTROY_FUNC destroy_func;
};

/* prototypes */
vt_check_t *vt_check_alloc (size_t, const char *);
int vt_check_weight (float);
int vt_check_sort (void *, void *);
int vt_check_dynamic_pattern (const char *);
char *vt_check_unescape_pattern (const char *);

#endif
