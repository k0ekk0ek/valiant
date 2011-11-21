#ifndef VT_MAP_PRIV_H_INCLUDED
#define VT_MAP_PRIV_H_INCLUDED 1

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "map.h"

vt_map_t *vt_map_create (cfg_t *, vt_errno_t *);
void vt_map_destroy (vt_map_t *);

#endif
