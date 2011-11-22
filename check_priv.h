#ifndef VT_CHECK_PRIV_H_INCLUDED
#define VT_CHECK_PRIV_H_INCLUDED 1

/* valiant includes */
#include "check.h"

vt_check_t *vt_check_create (const vt_map_list_t *, cfg_t *, vt_error_t *);
int vt_check_destroy (vt_check_t *, vt_error_t *);

#endif
