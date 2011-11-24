#ifndef VT_CHECK_H_INCLUDED
#define VT_CHECK_H_INCLUDED 1

/* system includes */
#include <confuse.h>
#include <sys/types.h>

/* valiant includes */
#include "error.h"
#include "map.h"
#include "request.h"
#include "score.h"

typedef struct _vt_check vt_check_t;

typedef int(*VT_CHECK_CHECK_FUNC)(vt_check_t *, vt_request_t *, vt_score_t *,
  vt_error_t *);
typedef int(*VT_CHECK_DESTROY_FUNC)(vt_check_t *, vt_error_t *);

struct _vt_check {
  unsigned int cntr; /* identifier used to keep statistics */
  char *name; /* check title */
  unsigned int prio; /* indicates check cost */
  void *data; /* check specific information */
  int *maps;
  int max_weight;
  int min_weight;
  VT_CHECK_CHECK_FUNC check_func;
  VT_CHECK_DESTROY_FUNC destroy_func;
};

#define VT_CHECK_TYPE_FLAG_NONE (0)
#define VT_CHECK_TYPE_FLAG_NEED_INIT (1<<1)

typedef struct _vt_check_type vt_check_type_t;

typedef int(*VT_CHECK_TYPE_INIT_FUNC)(cfg_t *, vt_error_t *);
typedef int(*VT_CHECK_TYPE_DEINIT_FUNC)(vt_error_t *);
typedef vt_check_t*(*VT_CHECK_TYPE_CREATE_CHECK_FUNC)(const vt_map_list_t *,
  cfg_t *, vt_error_t *);

struct _vt_check_type {
  char *name;
  unsigned int flags;
  VT_CHECK_TYPE_INIT_FUNC init_func;
  VT_CHECK_TYPE_DEINIT_FUNC deinit_func;
  VT_CHECK_TYPE_CREATE_CHECK_FUNC create_check_func;
};

int vt_check_sort (void *, void *);
int vt_check_dynamic_pattern (const char *);
char *vt_check_unescape_pattern (const char *);
int vt_check_weight (float);
int vt_check_types_init (cfg_t *, vt_check_type_t **, vt_error_t *);

#endif
