#ifndef VT_CHECK_PRIV_H_INCLUDED
#define VT_CHECK_PRIV_H_INCLUDED 1

/* system includes */
#include <confuse.h>
#include <sys/types.h>

/* valiant includes */
#include "error.h"
#include "check.h"

vt_check_t *vt_check_create (const vt_map_list_t *, cfg_t *, vt_errno_t **);
int vt_check_destroy (vt_check_t *, vt_errno_t **);

#endif

