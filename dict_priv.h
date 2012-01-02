#ifndef VT_DICT_PRIV_H_INCLUDED
#define VT_DICT_PRIV_H_INCLUDED 1

/* valiant includes */
#include "dict.h"
#include "thread_pool.h"

vt_dict_t *vt_dict_create_common (cfg_t *, vt_error_t *);
int vt_dict_destroy_common (vt_dict_t *, vt_error_t *);

#endif
