#ifndef VT_TYPE_H_INCLUDED
#define VT_TYPE_H_INCLUDED

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"

#define VT_TYPE_FLAG_NONE (0)
#define VT_TYPE_FLAG_NEED_INIT (1<<1)

typedef struct vt_type_struct vt_type_t;

typedef int(*VT_TYPE_INIT_FUNC)(const cfg_t *);
typedef int(*VT_TYPE_DEINIT_FUNC)(void);
typedef int(*VT_TYPE_CREATE_CHECK_FUNC)(vt_check_t **, const cfg_t *);

struct vt_type_struct {
  const char *name;
  unsigned int flags;
  const cfg_opt_t *type_opts;
  const cfg_opt_t *check_opts;
  VT_TYPE_INIT_FUNC init_func;
  VT_TYPE_DEINIT_FUNC deinit_func;
  VT_TYPE_CREATE_CHECK_FUNC create_check_func;
};

#endif
