#ifndef VT_CHECK_RHSBL_H_INCLUDED
#define VT_CHECK_RHSBL_H_INCLUDED

/* system includes */
#include <confuse.h>

/* valiant includes */
#include "check.h"

vt_check_type_t *vt_check_rhsbl_type (void);
int vt_check_rhsbl_init (const cfg_t *);
int vt_check_rhsbl_create (vt_check_t **, const cfg_t *);

#endif
